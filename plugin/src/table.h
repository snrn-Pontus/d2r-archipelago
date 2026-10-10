#pragma once

#include <string>
#include <vector>

namespace ap {

// A D2 excel table: tab-separated, first row is the header. Parse and Format
// round-trip byte for byte, including the line endings.
class Table {
public:
	static auto Parse(const std::string& text) -> Table;
	auto Format() const -> std::string;

	// -1 when the table has no such column.
	auto Column(const std::string& name) const -> int;
	auto Rows() -> std::vector<std::vector<std::string>>& { return rows_; }
	auto Rows() const -> const std::vector<std::vector<std::string>>& { return rows_; }
	// The cell, or an empty string when the row is short.
	auto Cell(size_t row, int column) const -> std::string;
	void Set(size_t row, int column, const std::string& value);

private:
	std::vector<std::string> header_;
	std::vector<std::vector<std::string>> rows_;
	std::string lineEnding_ = "\r\n";
	bool trailingNewline_ = true;
};

} // namespace ap
