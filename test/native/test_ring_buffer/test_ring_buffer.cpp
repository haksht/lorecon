/**
 * Native unit tests for StaticRingBuffer<T, N>.
 *
 * Tests the fixed-size ring buffer used for packet queueing.
 * Verifies push/pop, overflow rejection, wraparound, and size tracking.
 */
#include <unity.h>

void setUp(void) {}
void tearDown(void) {}

// StaticRingBuffer is a standalone template — pull it in directly
// to avoid the heavy include chain of packet_processor.h.
template<typename T, size_t N>
class StaticRingBuffer {
public:
    bool push(const T& item) {
        if (count_ >= N) return false;
        buf_[tail_] = item;
        tail_ = (tail_ + 1) % N;
        count_++;
        return true;
    }

    const T& front() const { return buf_[head_]; }

    void pop() {
        if (count_ == 0) return;
        head_ = (head_ + 1) % N;
        count_--;
    }

    bool empty() const { return count_ == 0; }
    size_t size() const { return count_; }

private:
    T buf_[N];
    size_t head_ = 0;
    size_t tail_ = 0;
    size_t count_ = 0;
};

// --- Basic operations ---

void test_empty_on_create() {
    StaticRingBuffer<int, 4> rb;
    TEST_ASSERT_TRUE(rb.empty());
    TEST_ASSERT_EQUAL(0, rb.size());
}

void test_push_and_front() {
    StaticRingBuffer<int, 4> rb;
    TEST_ASSERT_TRUE(rb.push(42));
    TEST_ASSERT_FALSE(rb.empty());
    TEST_ASSERT_EQUAL(1, rb.size());
    TEST_ASSERT_EQUAL(42, rb.front());
}

void test_push_pop_fifo_order() {
    StaticRingBuffer<int, 4> rb;
    rb.push(10);
    rb.push(20);
    rb.push(30);
    TEST_ASSERT_EQUAL(10, rb.front());
    rb.pop();
    TEST_ASSERT_EQUAL(20, rb.front());
    rb.pop();
    TEST_ASSERT_EQUAL(30, rb.front());
    rb.pop();
    TEST_ASSERT_TRUE(rb.empty());
}

// --- Overflow ---

void test_push_rejects_when_full() {
    StaticRingBuffer<int, 3> rb;
    TEST_ASSERT_TRUE(rb.push(1));
    TEST_ASSERT_TRUE(rb.push(2));
    TEST_ASSERT_TRUE(rb.push(3));
    TEST_ASSERT_FALSE(rb.push(4));  // Full — should reject
    TEST_ASSERT_EQUAL(3, rb.size());
    TEST_ASSERT_EQUAL(1, rb.front());  // First item preserved
}

// --- Wraparound ---

void test_wraparound() {
    StaticRingBuffer<int, 3> rb;
    // Fill and drain twice to force tail/head wraparound
    rb.push(1); rb.push(2); rb.push(3);
    rb.pop(); rb.pop(); rb.pop();
    TEST_ASSERT_TRUE(rb.empty());

    // Second fill — head and tail have wrapped
    TEST_ASSERT_TRUE(rb.push(4));
    TEST_ASSERT_TRUE(rb.push(5));
    TEST_ASSERT_EQUAL(4, rb.front());
    rb.pop();
    TEST_ASSERT_EQUAL(5, rb.front());
}

// --- Pop on empty ---

void test_pop_on_empty_is_safe() {
    StaticRingBuffer<int, 2> rb;
    rb.pop();  // Should not crash
    TEST_ASSERT_TRUE(rb.empty());
    TEST_ASSERT_EQUAL(0, rb.size());
}

// --- Size of 1 ---

void test_capacity_one() {
    StaticRingBuffer<int, 1> rb;
    TEST_ASSERT_TRUE(rb.push(99));
    TEST_ASSERT_FALSE(rb.push(100));
    TEST_ASSERT_EQUAL(99, rb.front());
    rb.pop();
    TEST_ASSERT_TRUE(rb.empty());
    TEST_ASSERT_TRUE(rb.push(100));
    TEST_ASSERT_EQUAL(100, rb.front());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_empty_on_create);
    RUN_TEST(test_push_and_front);
    RUN_TEST(test_push_pop_fifo_order);
    RUN_TEST(test_push_rejects_when_full);
    RUN_TEST(test_wraparound);
    RUN_TEST(test_pop_on_empty_is_safe);
    RUN_TEST(test_capacity_one);
    return UNITY_END();
}
