#pragma once

#include <minihttp/defines.hpp>
#include <minihttp/headers.hpp>
#include <minihttp/method.hpp>
#include <minihttp/status.hpp>
#include <minihttp/error.hpp>
#include <ilias/io/traits.hpp>
#include <ilias/io/stream.hpp>
#include <ilias/sync/event.hpp>
#include <charconv>
#include <optional>
#include <cassert>
#include <array>

namespace minihttp {

using ilias::Stream;

template <Stream T>
class Http1Stream;

/**
 * @brief The http1.1 connection, it only support one transaction at a time
 * 
 * @tparam T 
 */
template <Stream T>
class Http1Connection final {
public:
    explicit Http1Connection(T stream) : mStream(std::move(stream)) { mIdleEvent.set(); }
    Http1Connection(Http1Connection<T> &&) = delete;
    ~Http1Connection() = default;

    /**
     * @brief Create a new transaction stream for sending request
     * 
     * @return IoTask<Http1Stream<T> > 
     */
    auto sendStream() -> IoTask<Http1Stream<T> > {
        return newStream(false);
    }

    /**
     * @brief Create a new transaction stream for receiving response
     * 
     * @return IoTask<Http1Stream<T> > 
     */
    auto recvStream() -> IoTask<Http1Stream<T> > {
        return newStream(true);
    }

    /**
     * @brief Check if the connection is open
     * 
     * @return true 
     * @return false 
     */
    auto isOpen() const -> bool {
        return mIsOpen;
    }

    /**
     * @brief Get the next layer stream
     * 
     * @return T& 
     */
    auto nextLayer() -> T & {
        return mStream.nextLayer();
    }

    /**
     * @brief Get the event that will be set when the connection is idle
     * 
     * @return ilias::Event& 
     */
    auto idleEvent() -> ilias::Event & { 
        return mIdleEvent;
    }

    /**
     * @brief Detach the buffered stream, (this stream may have data in buffer, so you should check it, before you detach the T from the BufStream)
     * 
     * @return BufStream<T> 
     */
    auto detach() -> ilias::BufStream<T> {
        return std::move(mStream);
    }
private:
    auto newStream(bool server) -> IoTask<Http1Stream<T> >;

    ilias::BufStream<T> mStream;
    ilias::Event mIdleEvent;
    bool mIsOpen = true; // False when connection is closed, no-more transaction
template <Stream U>
friend class Http1Stream;
};

/**
 * @brief The single transaction stream
 *  Client-Send (writeRequest -> write -> writeEnd -> readResponse -> read) or 
 *  Server-Recv (readRequest -> read -> writeResponse -> write -> writeEnd)
 * 
 * @tparam T 
 */
template <Stream T>
class Http1Stream final : public ilias::StreamMethod<Http1Stream<T> > {
public:
    Http1Stream() = default;
    Http1Stream(const Http1Stream &) = delete;
    Http1Stream(Http1Stream<T> &&other);
    ~Http1Stream() { close(); }

    auto close() -> void;

    // Readable
    auto read(MutableBuffer buffer) -> IoTask<size_t>;
    auto readRequest(Method &method, std::string &path, Headers &header) -> IoTask<void>;
    auto readResponse(Status &status, Headers &header) -> IoTask<void>;

    // Writable
    auto write(Buffer buffer) -> IoTask<size_t>;
    auto flush() -> IoTask<void>;
    auto shutdown() -> IoTask<void>;

    auto writeEnd() -> IoTask<void>;
    auto writeRequest(Method method, std::string_view path, Headers &header) -> IoTask<void>;
    auto writeResponse(Status status, const Headers &header) -> IoTask<void>;

    auto operator =(const Http1Stream<T> &) -> Http1Stream<T> & = delete;
    auto operator =(Http1Stream<T> &&other) -> Http1Stream<T> &;

    explicit operator bool() const noexcept {
        return mCon != nullptr;
    }
private:
    explicit Http1Stream(Http1Connection<T> *con, bool server);

    auto stream() -> ilias::BufStream<T> & { return mCon->mStream; }
    auto readHeaders(Headers &header) -> IoTask<void>;
    auto writeHeaders(std::string line, const Headers &header) -> IoTask<void>;

    Http1Connection<T> *mCon = nullptr;

    // State
    std::optional<Method> mMethod;
    bool mKeepAlive = false;
    bool mServer = false; // Did this stream is used for server (recv mode)

    // Read fields
    struct {
        std::optional<size_t> contentLength; // Content-Length header, how many bytes left
        std::optional<size_t> chunkedSize; // Current chunk size
        size_t chunkLeft = 0; // How many bytes left in the current chunk
        bool   chunked = false; // Use chunked encoding
        bool   eof = false; // Did we finished reading?
    } mRead;

    // Write fields
    struct {
        std::optional<size_t> contentLength; // Content-Length header, how many bytes we can write
        bool headerEnd = false; // Did we fully write the headers?
        bool chunked = false; // Use chunked encoding
        bool payload = false; // Did this transaction has payload?
        bool end = false; // Did we finished writing?
    } mWrite;
template <Stream U>
friend class Http1Connection;
};


// Implmentation
template <Stream T>
auto Http1Connection<T>::newStream(bool server) -> IoTask<Http1Stream<T> > {
    if (!mIdleEvent.isSet()) {
        co_return Err(IoError::WouldBlock);
    }
    if (!mIsOpen) {
        co_return Err(IoError::ConnectionReset);
    }
    co_return Http1Stream<T>{this, server};
}

template <Stream T>
Http1Stream<T>::Http1Stream(Http1Connection<T> *con, bool server) : mCon(con), mServer(server) {
    mCon->mIdleEvent.clear(); // Not idle now
}

template <Stream T>
Http1Stream<T>::Http1Stream(Http1Stream<T> &&other) : 
    mCon(other.mCon),
    mMethod(other.mMethod),
    mKeepAlive(other.mKeepAlive),
    mServer(other.mServer),
    mRead(other.mRead),
    mWrite(other.mWrite)
{
    other.mCon = nullptr;
}

template <Stream T>
auto Http1Stream<T>::operator =(Http1Stream<T> &&other) -> Http1Stream<T> & {
    if (this == &other) {
        return *this;
    }
    close();
    mCon = other.mCon;
    mMethod = other.mMethod;
    mKeepAlive = other.mKeepAlive;
    mServer = other.mServer;
    mRead = other.mRead;
    mWrite = other.mWrite;
    other.mCon = nullptr;
    return *this;
}

// Close
template <Stream T>
auto Http1Stream<T>::close() -> void {
    if (!mCon) {
        return;
    }
    if (!mRead.eof || !mWrite.end) { // Not finished, so close the connection
        mCon->mIsOpen = false;
    }
    if (!mKeepAlive) { // Not keep-alive
        mCon->mIsOpen = false;
    }
    mCon->mIdleEvent.set(); // Idle now
    mCon = nullptr;
}

// Read
template <Stream T>
auto Http1Stream<T>::read(MutableBuffer buffer) -> IoTask<size_t> {
    if (mRead.eof || buffer.empty()) {
        co_return 0;
    }
    if (mRead.contentLength) { // Content-Length header
        auto len = *mRead.contentLength;
        auto min = std::min(len, buffer.size());
        if (min == 0) {
            mRead.eof = true;
            co_return 0;
        }
        auto res = co_await stream().read(buffer.subspan(0, min));
        if (res) {
            *mRead.contentLength -= res.value();
        }
        co_return res;
    }
    if (mRead.chunked) { // Chunked encoding
        size_t readed = 0;
        while (!buffer.empty()) {
            if (!mRead.chunkedSize) { // We need to read the chunk size
                auto line = co_await stream().getline("\r\n");
                if (!line) {
                    co_return Err(line.error());
                }
                size_t size = 0;
                if (std::from_chars(line->data(), line->data() + line->size(), size, 16).ec != std::errc()) {
                    co_return Err(HttpError::InvalidChunkFormat);
                }
                mRead.chunkedSize = size;
                mRead.chunkLeft = size;
            }
            // Read some data
            auto min = std::min(mRead.chunkLeft, buffer.size());
            if (min != 0) { // Have the space to read
                auto res = co_await stream().read(buffer.subspan(0, min));
                if (!res) {
                    co_return Err(res.error());
                }
                // Do the advance
                mRead.chunkLeft -= *res;
                readed += *res;
                buffer = buffer.subspan(*res);
            }
            if (mRead.chunkLeft == 0) { // Finished reading the chunk, skip the delimiter
                std::array<std::byte, 2> delim {};
                if (auto res = co_await stream().readAll(delim); !res) {
                    co_return Err(res.error());
                }
                if (delim != std::to_array({std::byte{'\r'}, std::byte{'\n'}})) {
                    co_return Err(HttpError::InvalidChunkFormat);
                }
                if (mRead.chunkedSize != 0) {
                    mRead.chunkedSize = std::nullopt;
                    continue; // Goto next chunk
                }
                mRead.eof = true;
                break;
            }
        }
        co_return readed;
    }
    if (!mKeepAlive) { // Legacy, read until EOF
        auto res = co_await stream().read(buffer);
        if (res == 0) {
            mRead.eof = true;
        }
        co_return res;
    }
    co_return Err(HttpError::InvalidState);
}

template <Stream T>
auto Http1Stream<T>::readRequest(Method &method, std::string &path, Headers &header) -> IoTask<void> {
    // GET / HTTP/1.1\r\n
    // K:V\r\n
    // \r\n
    auto line = co_await stream().getline("\r\n");
    if (!line) {
        co_return Err(line.error());
    }
    // Split by space
    {
        std::string_view view(*line);
        std::string_view m, p, v; // Method, path, version
        auto pos = view.find(' ');
        if (pos == std::string_view::npos) {
            co_return Err(HttpError::InvalidLine);
        }
        m = view.substr(0, pos);
        view.remove_prefix(pos + 1);

        pos = view.find(' ');
        if (pos == std::string_view::npos) {
            co_return Err(HttpError::InvalidLine);
        }
        p = view.substr(0, pos);
        view.remove_prefix(pos + 1);

        v = view;
        if (v != "HTTP/1.1") {
            co_return Err(HttpError::InvalidLine);
        }

        if (auto res = stringToMethod(m); res) {
            mMethod = *res;
            method = *res;
        }
        else {
            co_return Err(HttpError::InvalidLine);
        }

        path.assign(p);
    }
    // Headers left
    if (auto res = co_await readHeaders(header); !res) {
        co_return Err(res.error());
    }
    // Check if this request has body?
    if (mMethod == Method::Head || mMethod == Method::Get) {
        mRead.eof = true;
    }
    co_return {};
}

template <Stream T>
auto Http1Stream<T>::readResponse(Status &status, Headers &header) -> IoTask<void> {
    // HTTP/1.1 200 OK\r\n
    // K:V\r\n
    // \r\n
    if (!mWrite.end) { // The request must be send, otherwise we can't read the response
        co_return Err(HttpError::InvalidState);
    }
    auto line = co_await stream().getline("\r\n");
    if (!line) {
        co_return Err(line.error());
    }
    // Split by space
    {
        std::string_view view(*line);
        std::string_view v, s, r; // Version, status, reason
        auto pos = view.find(' ');
        if (pos == std::string_view::npos) {
            co_return Err(HttpError::InvalidLine);
        }
        v = view.substr(0, pos);
        view.remove_prefix(pos + 1);

        pos = view.find(' ');
        if (pos == std::string_view::npos) {
            co_return Err(HttpError::InvalidLine);
        }
        s = view.substr(0, pos); // Status
        view.remove_prefix(pos + 1);

        r = view; // Reason
        if (v != "HTTP/1.1") {
            co_return Err(HttpError::InvalidLine);
        }

        // Parse the status
        int statusCode = 0;
        if (std::from_chars(s.data(), s.data() + s.size(), statusCode).ec != std::errc{}) {
            co_return Err(HttpError::InvalidLine);
        }
        status = static_cast<Status>(statusCode);
    }
    // Headers left
    if (auto res = co_await readHeaders(header); !res) {
        co_return Err(res.error());
    }
    // Check if this response has body?
    if (mMethod == Method::Head) {
        mRead.eof = true;
    }
    if (status == Status::NoContent || status == Status::NotModified || toKind(status) == StatusKind::Informational) { // 1xx or 204 or 304
        mRead.eof = true;
    }
    co_return {};
}

// Write
template <Stream T>
auto Http1Stream<T>::write(Buffer buffer) -> IoTask<size_t> {
    using namespace ilias::literals; // _bin
    if (buffer.empty()) {
        co_return 0;
    }
    if (!mWrite.headerEnd) {
        if (auto res = co_await stream().writeAll("Transfer-Encoding: chunked\r\n\r\n"_bin); !res) {
            co_return Err(res.error());
        }
        mWrite.chunked = true;
        mWrite.headerEnd = true;
    }
    if (mWrite.contentLength) { // The user specified the Content-Length, use it
        assert(mWrite.headerEnd && !mWrite.chunked);
        auto left = std::min(buffer.size(), *mWrite.contentLength);
        auto res = co_await stream().writeAll(buffer.subspan(0, left));
        if (!res) {
            co_return Err(res.error());
        }
        *mWrite.contentLength -= *res;
        co_return *res;
    }
    else { // Chunked mode, hex\r\n\buffer\r\n
        char buf[18] {0}; // uint64_t to hex, max 16 chars + "\r\n"
        auto [end, ec] = std::to_chars(buf, buf + sizeof(buf), buffer.size(), 16);
        if (ec != std::errc{}) { // Why?
            co_return Err(IoError::Other);
        }
        *(end++) = '\r';
        *(end++) = '\n';
        if (auto res = co_await stream().writeAll(ilias::makeBuffer(buf, end - buf)); !res) {
            co_return Err(res.error());   
        }
        if (auto res = co_await stream().writeAll(buffer); !res) {
            co_return Err(res.error());
        }
        if (auto res = co_await stream().writeAll("\r\n"_bin); !res) {
            co_return Err(res.error());
        }
        co_return buffer.size(); // All bytes are written
    }
}

template <Stream T>
auto Http1Stream<T>::writeEnd() -> IoTask<void> {
    using namespace ilias::literals; // _bin
    if (!mWrite.headerEnd) {
        auto buffer = "\r\n"_bin;
        if (mServer) { // If we are server
            if (mMethod != Method::Head) { // Check this method can have body and then mark it as empty
                buffer = "Content-Length: 0\r\n\r\n"_bin; // No body
            }
        }
        if (auto res = co_await stream().writeAll(buffer); !res) {
            co_return Err(res.error());
        }
    }
    if (mWrite.chunked) {
        // Write the final chunk
        if (auto res = co_await stream().writeAll("0\r\n\r\n"_bin); !res) {
            co_return Err(res.error());
        }
    }
    mWrite.end = true;
    co_return {};
}

template <Stream T>
auto Http1Stream<T>::writeRequest(Method method, std::string_view path, Headers &header) -> IoTask<void> {
    // Method path HTTP/1.1\r\n
    // K:V\r\n
    // Add the final \r\n in writeEnd()
    std::string line;
    line.reserve(path.size() + 64);
    line += toString(method);
    line += ' ';
    line += path;
    line += " HTTP/1.1\r\n";
    mMethod = method;
    co_return co_await writeHeaders(std::move(line), header);
}

template <Stream T>
auto Http1Stream<T>::writeResponse(Status status, const Headers &header) -> IoTask<void> {
    // HTTP/1.1 Status Reaosn\r\n
    // K:V\r\n
    std::string line;
    line += "HTTP/1.1 ";
    line += std::to_string(int(status));
    line += ' ';
    line += toString(status);
    line += "\r\n";
    co_return co_await writeHeaders(std::move(line), header);
}

template <Stream T>
auto Http1Stream<T>::flush() -> IoTask<void> {
    return stream().flush();
}

template <Stream T>
auto Http1Stream<T>::shutdown() -> IoTask<void> {
    co_return {}; // nothing to do now
}

// Internal
template <Stream T>
auto Http1Stream<T>::readHeaders(Headers &header) -> IoTask<void> {
    auto trim = [](std::string_view view) -> std::string_view {
        auto begin = view.find_first_not_of(' ');
        auto end = view.find_last_not_of(' ');
        if (begin == std::string_view::npos || end == std::string_view::npos) {
            return {};
        }
        return view.substr(begin, end - begin + 1);
    };
    std::string line;
    while (true) {
        line.clear();
        if (auto res = co_await stream().readline(line, "\r\n"); !res) {
            co_return Err(res.error());
        }
        auto view = std::string_view(line);
        if (view.empty()) {
            co_return Err(IoError::UnexpectedEOF);
        }
        if (view == "\r\n") { // End of headers
            break;
        }
        view.remove_suffix(2); // Remove \r\n
        auto pos = view.find(':');
        if (pos == std::string_view::npos) {
            co_return Err(HttpError::InvalidHeader);
        }
        auto key = trim(view.substr(0, pos));
        auto val = trim(view.substr(pos + 1));
        header.append(key, val);
    }
    // Check for Content-Length, Connection, Transfer-Encoding
    if (header.value(Headers::Connection) != "close") { // If `Connection` is not exisit, that is implicitly keep-alive
        mKeepAlive = true;
    }
    if (header.value(Headers::TransferEncoding) == "chunked") {
        mRead.chunked = true;
    }
    else if (auto str = header.value(Headers::ContentLength); !str.empty()) {
        size_t len = 0;
        if (std::from_chars(str.data(), str.data() + str.size(), len).ec != std::errc{}) {
            co_return Err(HttpError::InvalidHeader);
        }
        if (len == 0) {
            mRead.eof = true;
        }
        mRead.contentLength = len;
    }
    co_return {};
}

template <Stream T>
auto Http1Stream<T>::writeHeaders(std::string line, const Headers &header) -> IoTask<void> {
    // Add the final \r\n in writeEnd() if needed
    for (auto &[key, val] : header) {
        line += key;
        line += ": ";
        line += val;
        line += "\r\n";
    }
    if (header.value(Headers::Connection).empty()) {
        line += "Connection: keep-alive\r\n";
    }
    if (header.value(Headers::TransferEncoding) == "chunked") {
        mWrite.chunked = true;
        mWrite.headerEnd = true;
        line += "\r\n";
    }
    else if (auto str = header.value(Headers::ContentLength); !str.empty()) { // User specified Content-Length
        size_t len = 0;
        if (std::from_chars(str.data(), str.data() + str.size(), len).ec != std::errc{}) {
            co_return Err(HttpError::InvalidHeader);
        }
        mWrite.contentLength = len;
        mWrite.payload = true;
        mWrite.headerEnd = true;
        line += "\r\n";
    }
    if (auto res = co_await stream().writeAll(ilias::makeBuffer(line)); !res) {
        co_return Err(res.error());
    }
    co_return {};
}

} // namespace minihttp