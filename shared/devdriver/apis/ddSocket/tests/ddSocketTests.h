#pragma once

#include <ddSocket.h>
#include <ddSocket.h> // Test the include guards

#include <ddTestUtil.h>

/// A fixture specialized for testing single-threaded transfers
/// Takes a parameter for the transfer size in bytes
/// Note: Zero is not a valid transfer size for this test
class ddSocketSingleThreadedTransferTest : public DDNetworkedTest, public ::testing::WithParamInterface<size_t>
{
};

/// A fixture specialized for testing multi-threaded transfers
/// Takes a parameter for the transfer size in bytes
/// Note: Zero is not a valid transfer size for this test
class ddSocketMultiThreadedTransferTest : public DDNetworkedTest, public ::testing::WithParamInterface<size_t>
{
};

/// A fixture specialized for testing variable chunk sizes (variable read/write sizes)
/// Takes a two parameter tuple for the read and write chunk sizes in bytes respectively
/// Note: Zero is not a valid parameter size size for this test
class ddSocketVariableChunkSizesTest : public DDNetworkedTest, public ::testing::WithParamInterface<std::tuple<size_t, size_t>>
{
};
