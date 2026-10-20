// SPDX-License-Identifier: MIT
#pragma once
#include "library_core.hpp"
#include <cstddef>
#include <vector>
namespace ulg {
// Optional read-only adapter to the user's ALREADY RUNNING ftpsrv on 127.0.0.1:2121.
// Does not start, install or configure any service or send mutating FTP commands.
void supplement_from_local_ftp(Catalogue &catalogue,const std::vector<Root> &roots,
                               int max_depth=5,std::size_t max_games=5000,int port=2121, const std::string &cover_dir="");
} // namespace ulg
