#ifndef _TEST_TYPES_H_
#define _TEST_TYPES_H_

struct movable_type {
	int value;

	movable_type() = default;
	explicit movable_type(int value) : value(value) {}
	movable_type(const movable_type &) = delete;
	movable_type(movable_type &&) = default;
	movable_type &operator=(const movable_type &) = delete;
};

struct hash_movable_type {

	auto operator()(const movable_type &movable) const {
		return std::size_t(movable.value);
	}
};

struct odd_comparator_type {

	bool operator()(int lhs, int rhs) const {
		return lhs % 2 == rhs % 2;
	}
};

struct movable_odd_comparator_type {

	bool operator()(const movable_type &lhs, const movable_type &rhs) const {
		return comparator(lhs.value, rhs.value);
	}

	odd_comparator_type comparator;
};

#endif // _TEST_TYPES_H_
