// This file Copyright (C) 2026 Mnemosyne LLC.
// It may be used under GPLv2 (SPDX: GPL-2.0-only), GPLv3 (SPDX: GPL-3.0-only),
// or any future license endorsed by Mnemosyne LLC.
// License text can be found in the licenses/ folder.

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>

#ifndef _WIN32
#include <sys/stat.h>
#endif

#include <gtest/gtest.h>

#include <libtransmission/transmission.h>

#include <libtransmission/torrent.h>
#include <libtransmission/tr-strbuf.h>

#include "test-fixtures.h"

namespace tr::test
{

using TorrentTest = SessionTest;

namespace
{

size_t count_script_runs(std::string const& filename)
{
    auto in = std::ifstream{ filename };
    if (!in)
    {
        return 0U;
    }

    return static_cast<size_t>(std::count(std::istreambuf_iterator<char>{ in }, std::istreambuf_iterator<char>{}, '\n'));
}

} // namespace

TEST_F(TorrentTest, doneScriptNotTriggeredTwiceAfterRecheck)
{
#ifdef _WIN32
    GTEST_SKIP();
#else
    auto const script_path = tr_pathbuf{ sandboxDir(), "/done-script.sh" };
    auto const runs_path = tr_pathbuf{ sandboxDir(), "/done-script-runs.txt" };

    auto script = std::string{};
    script += "#!/bin/sh\n";
    script += "printf 'run\\n' >> \"";
    script += runs_path.sv();
    script += "\"\n";

    createFileWithContents(script_path.sv(), script);
    ASSERT_EQ(0, chmod(script_path.c_str(), 0700));

    tr_sessionSetScript(session_, TR_SCRIPT_ON_TORRENT_DONE, script_path.sv());
    tr_sessionSetScriptEnabled(session_, TR_SCRIPT_ON_TORRENT_DONE, true);

    auto* const tor = zeroTorrentInit(ZeroTorrentState::Partial);
    ASSERT_NE(nullptr, tor);
    EXPECT_FALSE(tor->is_done());
    EXPECT_FALSE(tor->done_script_called());

    for (tr_piece_index_t piece = 0, n = tor->piece_count(); piece < n; ++piece)
    {
        tor->set_has_piece(piece, true);
    }
    tor->recheck_completeness();

    EXPECT_TRUE(tor->is_done());
    EXPECT_TRUE(tor->done_script_called());
    ASSERT_TRUE(waitFor([&runs_path]() { return count_script_runs(std::string{ runs_path.sv() }) == 1U; }, 5000));

    tor->set_has_piece(0, false);
    tor->recheck_completeness();

    EXPECT_FALSE(tor->is_done());
    EXPECT_TRUE(tor->done_script_called());

    tor->set_has_piece(0, true);
    tor->recheck_completeness();

    EXPECT_TRUE(tor->is_done());
    EXPECT_TRUE(tor->done_script_called());
    EXPECT_FALSE(waitFor([&runs_path]() { return count_script_runs(std::string{ runs_path.sv() }) > 1U; }, 1000));
    EXPECT_EQ(1U, count_script_runs(std::string{ runs_path.sv() }));
#endif
}

} // namespace tr::test
