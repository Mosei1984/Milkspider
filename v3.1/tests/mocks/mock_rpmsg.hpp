#ifndef MOCK_RPMSG_HPP
#define MOCK_RPMSG_HPP

#include <cstdint>
#include <cstring>
#include <queue>
#include <vector>

/**
 * MockRpmsg - Simulates RPMsg send/receive with queue
 */
class MockRpmsg {
public:
    static constexpr size_t MAX_MSG_SIZE = 512;

    struct Message {
        std::vector<uint8_t> data;
        uint64_t timestamp_ms;
    };

    MockRpmsg() {
        reset();
    }

    void reset() {
        while (!m_tx_queue.empty()) m_tx_queue.pop();
        while (!m_rx_queue.empty()) m_rx_queue.pop();
        m_init_called = false;
        m_close_called = false;
        m_tx_count = 0;
        m_rx_count = 0;
    }

    bool init() {
        m_init_called = true;
        return true;
    }

    void close() {
        m_close_called = true;
    }

    bool send(const void *data, size_t len) {
        if (len > MAX_MSG_SIZE) return false;

        Message msg;
        msg.data.resize(len);
        memcpy(msg.data.data(), data, len);
        msg.timestamp_ms = m_tick_count;

        m_tx_queue.push(msg);
        m_tx_count++;

        return true;
    }

    int receive(void *buffer, size_t max_len) {
        if (m_rx_queue.empty()) return 0;

        Message msg = m_rx_queue.front();
        m_rx_queue.pop();

        size_t copy_len = (msg.data.size() < max_len) ? msg.data.size() : max_len;
        memcpy(buffer, msg.data.data(), copy_len);
        m_rx_count++;

        return (int)copy_len;
    }

    void injectRx(const void *data, size_t len) {
        Message msg;
        msg.data.resize(len);
        memcpy(msg.data.data(), data, len);
        msg.timestamp_ms = m_tick_count;
        m_rx_queue.push(msg);
    }

    bool hasTxData() const { return !m_tx_queue.empty(); }
    bool hasRxData() const { return !m_rx_queue.empty(); }

    Message popTx() {
        Message msg = m_tx_queue.front();
        m_tx_queue.pop();
        return msg;
    }

    size_t getTxCount() const { return m_tx_count; }
    size_t getRxCount() const { return m_rx_count; }

    bool wasInitCalled() const { return m_init_called; }
    bool wasCloseCalled() const { return m_close_called; }

    void tick() { m_tick_count++; }

private:
    std::queue<Message> m_tx_queue;
    std::queue<Message> m_rx_queue;
    uint64_t m_tick_count = 0;
    size_t m_tx_count = 0;
    size_t m_rx_count = 0;
    bool m_init_called = false;
    bool m_close_called = false;
};

#endif // MOCK_RPMSG_HPP
