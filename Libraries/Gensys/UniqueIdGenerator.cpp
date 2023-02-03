#include "UniqueIdGenerator.h"

using namespace Gensys;

static int mUniqueId;

int UniqueIdGenerator::GetUniqueId()
{
	return ++mUniqueId;
}