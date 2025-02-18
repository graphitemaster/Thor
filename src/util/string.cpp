#include "util/string.h"

namespace Thor {

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
void StringBuilder::lpad(Ulen n, char ch, char pad) {
	if (n) for (Ulen i = 0; i < n - 1; i++) put(pad);
	put(ch);
}

void StringBuilder::lpad(Ulen n, StringView view, char pad) {
	const auto l = view.length();
	if (n >= l) for (Ulen i = 0; i < n - l; i++) put(pad);
	put(view);
}

void StringBuilder::rpad(Ulen n, char ch, char pad) {
	put(ch);
	if (n) for (Ulen i = 0; i < n - 1; i++) put(pad);
}

void StringBuilder::rpad(Ulen n, StringView view, char pad) {
	const auto l = view.length();
	put(view);
	if (n >= l) for (Ulen i = 0; i < n - l; i++) put(pad);
}

Maybe<StringView> StringBuilder::result() const {
	if (error_) {
		return {};
	}
	return StringView { build_.slice() };
}

} // namespace Thor