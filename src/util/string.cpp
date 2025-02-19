#include <string.h> // TODO(dweiler): remove

#include "util/string.h"

namespace Thor {

// StringBuilder
void StringBuilder::put(char ch) {
	error_ = !build_.push_back(ch);
}

void StringBuilder::put(StringView view) {
	const auto offset = build_.length();
	if (build_.resize(offset + view.length())) {
		const auto len = view.length();
		for (Ulen i = 0; i < len; i++) {
			build_[offset + i] = view[i];
		}
	} else {
		error_ = true;
	}
}

void StringBuilder::rep(Ulen n, char ch) {
	for (Ulen i = 0; i < n; i++) put(ch);
}

void StringBuilder::lpad(Ulen n, char ch, char pad) {
	if (n) rep(n - 1, pad);
	put(ch);
}

void StringBuilder::lpad(Ulen n, StringView view, char pad) {
	const auto l = view.length();
	if (n >= l) rep(n - 1, pad);
	put(view);
}

void StringBuilder::rpad(Ulen n, char ch, char pad) {
	put(ch);
	if (n) rep(n - 1, pad);
}

void StringBuilder::rpad(Ulen n, StringView view, char pad) {
	const auto l = view.length();
	put(view);
	if (n >= l) rep(n - l, pad);
}

Maybe<StringView> StringBuilder::result() const {
	if (error_) {
		return {};
	}
	return StringView { build_.slice() };
}

// StringTable
StringTable::StringTable(StringTable&& other)
	: map_{move(other.map_)}
	, data_{exchange(other.data_, nullptr)}
	, capacity_{exchange(other.capacity_, 0)}
	, length_{exchange(other.length_, 0)}
{
}

StringRef StringTable::insert(StringView src) {
	if (src.length() >= 0xff'ff'ff'ff_u32) {
		// Cannot handle strings larger than 4 GiB.
		return {};
	}
	if (auto find = map_.find(src)) {
		// Duplicate string found, reuse it.
		return StringRef { find->v };
	}
	if (length_ + src.length() >= capacity_ && !grow(src.length())) {
		// Out of memory.
		return {};
	}
	StringRef ref { Uint32(length_), Uint32(src.length()) };
	StringView dst { data_ + length_, src.length() };
	memcpy(data_ + length_, src.data(), src.length());
	if (map_.insert(dst, ref)) {
		length_ += src.length();
		return ref;
	}
	return {};
}

Bool StringTable::grow(Ulen additional) {
	Map<StringView, StringRef> map{allocator()};
	auto old_capacity = capacity_;
	auto new_capacity = old_capacity ? old_capacity : 1;
	while (length_ + additional >= new_capacity) {
		new_capacity *= 2;
	}
	auto data = allocator().allocate<char>(new_capacity, true);
	if (!data) {
		return false;
	}
	for (const auto kv : map_) {
		auto k = kv.k;
		auto v = kv.v;
		auto beg = data + v.offset;
		memcpy(beg, k.data(), k.length());
		map.insert(StringView { beg, v.length }, v);
	}
	drop();
	map_ = move(map);
	data_ = data;
	capacity_ = new_capacity;
	return true;
}

} // namespace Thor