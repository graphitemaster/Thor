#ifndef THOR_STRING_H
#define THOR_STRING_H
#include "util/array.h"
#include "util/maybe.h"
#include "util/map.h"

namespace Thor {

using StringView = Slice<const char>;

// Utility for building a string incrementally.
struct StringBuilder {
	constexpr StringBuilder(Allocator& allocator)
		: build_{allocator}
	{
	}
	void put(char ch);
	void put(StringView view);
	THOR_FORCEINLINE void put(Float32 v) { put(Float64(v)); }
	void put(Float64 v);
	THOR_FORCEINLINE void put(Uint8 v) { put(Uint16(v)); }
	THOR_FORCEINLINE void put(Uint16 v) { put(Uint32(v)); }
	THOR_FORCEINLINE void put(Uint32 v) { put(Uint64(v)); }
	void put(Uint64 v);
	void put(Sint64 v);
	THOR_FORCEINLINE void put(Sint32 v) { put(Sint64(v)); }
	THOR_FORCEINLINE void put(Sint16 v) { put(Sint32(v)); }
	THOR_FORCEINLINE void put(Sint8 v) { put(Sint16(v)); }
	void rep(Ulen n, char ch = ' ');
	void lpad(Ulen n, char ch, char pad = ' ');
	void lpad(Ulen n, StringView view, char pad = ' ');
	void rpad(Ulen n, char ch, char pad = ' ');
	void rpad(Ulen n, StringView view, char pad = ' ');
	void reset();
	Maybe<StringView> result() const;
	StringView last() const { return last_; } // Last inserted string token
private:
	Array<char> build_;
	Bool        error_ = false;
	StringView  last_;
};

struct StringRef {
	constexpr StringRef() = default;
	constexpr StringRef(Unit) : StringRef{}{}
	constexpr StringRef(Uint32 offset, Uint32 length)
		: offset{offset}
		, length{length}
	{
	}
	Uint32 offset = 0;
	Uint32 length = ~0_u32;
	THOR_FORCEINLINE constexpr auto is_valid() const {
		return length != ~0_u32;
	}
	THOR_FORCEINLINE constexpr operator Bool() const {
		return is_valid();
	}
};

// Limited to no larger than 4 GiB of string data. Odin source files are limited
// to 2 GiB so this shouldn't ever be an issue as the StringTable represents an
// interned representation of identifiers in a single Odin source file.
struct Stream;

struct StringTable {
	constexpr StringTable(Allocator& allocator)
		: map_{allocator}
	{
	}
	
	static Maybe<StringTable> load(Allocator& allocator, Stream& stream);
	Bool save(Stream& stream) const;

	StringTable(StringTable&& other);

	StringTable& operator=(StringTable&& other) {
		return *new (drop(), Nat{}) StringTable{move(other)};
	}

	~StringTable() { drop(); }

	[[nodiscard]] StringRef insert(StringView src);

	THOR_FORCEINLINE constexpr StringView operator[](StringRef ref) const {
		return StringView { data_ + ref.offset, ref.length };
	}

	THOR_FORCEINLINE constexpr Allocator& allocator() {
		// We don't store a copy of the allocator since it's the same one used for
		// both map_ and data_ and since map_ already stores the allocator we can
		// just read it from there.
		return map_.allocator();
	}

	Slice<char> data() const { return { data_, length_ }; }

private:
	constexpr StringTable(Allocator& allocator, char* data, Uint32 length)
		: map_{allocator}
		, data_{data}
		, capacity_{length}
		, length_{length}
	{
	}

	StringTable* drop() {
		allocator().deallocate(data_, capacity_);
		return this;
	}

	[[nodiscard]] Bool grow(Ulen additional);

	Map<StringView, StringRef> map_;
	char*                      data_     = nullptr;
	Uint32                     capacity_ = 0;
	Uint32                     length_   = 0;
};

} // namespace Thor

#endif // THOR_STRING_H
