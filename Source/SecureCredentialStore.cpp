#include "SecureCredentialStore.h"

SecureCredentialStore& SecureCredentialStore::instance()
{
    static SecureCredentialStore store;
    return store;
}
