#include "pak.hpp"

#include <Dune2/io.hpp>

#include <fstream>
#include <istream>
#include <optional>

namespace std {

namespace {

uintmax_t
readOffset(istream &in) {
    using nr::dune2::io::readInteger;
    const auto offset = readInteger<4, uintmax_t>(in).value_or(0);
    return offset > 0
        ? offset
        : static_cast<uintmax_t>(in.seekg(0, std::ios::end).tellg());
}
} // namespace

std::istream &
operator>>(std::istream &in, std::pair<uintmax_t, string> &entry) {
    if (in) {
        entry = std::make_pair(
            readOffset(in),
            nr::dune2::io::readString(in)
        );
    }
    return in;
}

std::ostream &
operator<<(std::ostream &out, const nr::dune2::PAK::Entry &entry) {
    auto buf = entry.buffer();
    out << &buf;
    return out;
}
} // namespace std

namespace nr::dune2 {

///////////////////////////////////////////////////////////////////////////////
// PAK
struct PAK::impl {
    using RawEntry = std::pair<std::uintmax_t, std::string>;
    const std::ios::openmode OPEN_MODE = std::ios::binary|std::ios::in;

    const PAK &owner;

    std::shared_ptr<std::fstream> input;
    std::vector<Entry> entries;

    impl(const PAK &owner)
        : owner{owner} {
    }

    impl(const PAK &owner, const std::string &filepath)
        : owner{owner} {
        load(filepath);
    }

    bool
    load(const std::string &filepath) {
        entries.clear();
        input.reset();

        const auto stream = std::make_shared<std::fstream>(filepath, OPEN_MODE);

        if (*stream) {
            std::vector<RawEntry> raw_entries;

            std::copy(
                std::istream_iterator<RawEntry>(*stream),
                std::istream_iterator<RawEntry>(),
                std::back_inserter(raw_entries)
            );

            std::transform(
                std::begin(raw_entries),
                std::end(raw_entries) - 1,
                std::begin(raw_entries) + 1,
                std::back_inserter(entries),
                [&](const auto &entry1, const auto &entry2) {
                    return Entry{
                        .owner = owner,
                        .name = entry1.second,
                        .offset = entry1.first,
                        .size = entry2.first - entry1.first
                    };
                }
            );

            input = stream;
        }

        return input && !input->bad();
    }
};

PAK::PAK()
    : pimpl_{std::make_unique<impl>(*this)} {
}

PAK::~PAK() {
}

bool
PAK::load(const std::string &filepath) {
    return pimpl_->load(filepath);
}

PAK::const_iterator
PAK::begin() const {
    return pimpl_->entries.begin();
}

PAK::const_iterator
PAK::end() const {
    return pimpl_->entries.end();
}

PAK::const_iterator
PAK::cbegin() const {
    return begin();
}

PAK::const_iterator
PAK::cend() const {
    return end();
}

///////////////////////////////////////////////////////////////////////////////
// PAK::EntryBuffer

struct PAK::EntryBuffer::impl {
    std::weak_ptr<std::fstream> input;
    const PAK::Entry entry;
    uintmax_t pos;
    char ch;

    impl(const PAK::Entry &entry)
        : entry(entry)
        , pos{0}
        , ch{0} {
    }
};

PAK::EntryBuffer::EntryBuffer(const PAK::Entry &entry)
    : pimpl_(std::make_unique<impl>(entry)) {
    pimpl_->input = entry.owner.pimpl_->input;
}

std::streambuf::int_type
PAK::EntryBuffer::underflow() {
    const auto _EOF = std::char_traits<char>::eof();
    const auto input = pimpl_->input.lock();
    if (input && pimpl_->pos < pimpl_->entry.size) {
        input->clear();
        input->seekg(pimpl_->entry.offset + pimpl_->pos);
        if (*input) {
            pimpl_->ch = input->get();
            pimpl_->pos +=  1;
        }
    } else {
        pimpl_->ch = _EOF;
    }
    setg(&pimpl_->ch, &pimpl_->ch, &pimpl_->ch + 1);
    return pimpl_->ch;
}

///////////////////////////////////////////////////////////////////////////////
// PAK::Entry

PAK::EntryBuffer
PAK::Entry::buffer() const {
    return EntryBuffer(*this);
}

} // namespace nr::dune2
