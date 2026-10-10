#include "table.h"

namespace ap {

namespace {

auto Split(const std::string& line) -> std::vector<std::string> {
	std::vector<std::string> fields;
	size_t start = 0;
	for (size_t tab = line.find('\t'); tab != std::string::npos; tab = line.find('\t', start)) {
		fields.push_back(line.substr(start, tab - start));
		start = tab + 1;
	}
	fields.push_back(line.substr(start));
	return fields;
}

auto Join(const std::vector<std::string>& fields) -> std::string {
	std::string line;
	for (size_t i = 0; i < fields.size(); ++i) {
		if (i > 0) {
			line += '\t';
		}
		line += fields[i];
	}
	return line;
}

} // namespace

auto Table::Parse(const std::string& text) -> Table {
	Table table;
	table.lineEnding_ = text.find("\r\n") != std::string::npos ? "\r\n" : "\n";
	table.trailingNewline_ = !text.empty() && text.back() == '\n';
	size_t start = 0;
	bool header = true;
	while (start < text.size()) {
		size_t end = text.find('\n', start);
		if (end == std::string::npos) {
			end = text.size();
		}
		std::string line = text.substr(start, end - start);
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		if (header) {
			table.header_ = Split(line);
			header = false;
		} else {
			table.rows_.push_back(Split(line));
		}
		start = end + 1;
	}
	return table;
}

auto Table::Format() const -> std::string {
	std::string text = Join(header_);
	for (const auto& row : rows_) {
		text += lineEnding_ + Join(row);
	}
	if (trailingNewline_) {
		text += lineEnding_;
	}
	return text;
}

auto Table::Column(const std::string& name) const -> int {
	for (size_t i = 0; i < header_.size(); ++i) {
		if (header_[i] == name) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

auto Table::Cell(size_t row, int column) const -> std::string {
	if (column < 0 || row >= rows_.size() || static_cast<size_t>(column) >= rows_[row].size()) {
		return {};
	}
	return rows_[row][static_cast<size_t>(column)];
}

void Table::Set(size_t row, int column, const std::string& value) {
	if (column < 0 || row >= rows_.size()) {
		return;
	}
	auto& fields = rows_[row];
	if (static_cast<size_t>(column) >= fields.size()) {
		fields.resize(static_cast<size_t>(column) + 1);
	}
	fields[static_cast<size_t>(column)] = value;
}

} // namespace ap
