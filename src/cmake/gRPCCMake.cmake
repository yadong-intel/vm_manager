#
# Copyright (c) 2022 Intel Corporation.
# All rights reserved.
#
# SPDX-License-Identifier: Apache-2.0
#
#

include(FetchContent)
FetchContent_Declare(
  gRPC
  GIT_REPOSITORY https://github.com/grpc/grpc
  GIT_TAG        v1.46.1
)
set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(gRPC)