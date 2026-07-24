#ifndef _SPLIT_BY_CHAR_H_
#define _SPLIT_BY_CHAR_H_

#include <iterator>
#include <type_traits>

#include "compat-string_view.h"

class SplitByCharIterator {
 public:
	using iterator_category = std::forward_iterator_tag;
	using value_type = compat::string_view;
	using reference = std::add_lvalue_reference<value_type>::type;
	using pointer = std::add_pointer<value_type>::type;

	static SplitByCharIterator begin(compat::string_view text, char split_by) {
		return SplitByCharIterator(split_by, text, text.substr(0, text.find(split_by)));
	}

	static SplitByCharIterator end(compat::string_view text, char split_by) {
		return SplitByCharIterator(split_by, text, text.substr(text.size()));
	}

	compat::string_view operator*() const { return slice_; }

	const compat::string_view *operator->() const { return &slice_; }

	SplitByCharIterator &operator++() {
		slice_ = text_.substr(slice_.data() - text_.data() + slice_.size());
		if (!slice_.empty()) slice_.remove_prefix(1);  // skip the split_by char
		slice_ = slice_.substr(0, slice_.find(split_by_));
    return *this;
	}

	SplitByCharIterator operator++(int) const {
		auto copy = *this;
		++copy;
		return copy;
	}

	bool operator==(const SplitByCharIterator &rhs) const {
		return slice_.data() == rhs.slice_.data();
	}

	bool operator!=(const SplitByCharIterator &rhs) const {
		return !(*this == rhs);
	}

 private:
	SplitByCharIterator(char split_by, compat::string_view text, compat::string_view slice)
	    : split_by_(split_by), text_(text), slice_(slice) {
	}

	const char split_by_;
	const compat::string_view text_;
	compat::string_view slice_;
};

class SplitByChar {
 public:
	explicit SplitByChar(compat::string_view text, char split_by)
	    : text_(text), split_by_(split_by) {}

	SplitByCharIterator begin() const {
		return SplitByCharIterator::begin(text_, split_by_);
	}

	SplitByCharIterator end() const {
		return SplitByCharIterator::end(text_, split_by_);
	}

 private:
	const compat::string_view text_;
	const char split_by_;
};

#endif // _SPLIT_BY_CHAR_H_
