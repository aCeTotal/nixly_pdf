#include "context.h"

#include <mutex>

namespace {

std::mutex locks[FZ_LOCK_MAX];

void lockMutex(void *, int lock)
{
    locks[lock].lock();
}

void unlockMutex(void *, int lock)
{
    locks[lock].unlock();
}

struct ThreadClone
{
    fz_context *ctx = fz_clone_context(mainContext());
    ~ThreadClone() { fz_drop_context(ctx); }
};

} // namespace

fz_context *mainContext()
{
    static const fz_locks_context lockTable{nullptr, lockMutex, unlockMutex};
    static fz_context *ctx = [] {
        fz_context *created = fz_new_context(nullptr, &lockTable, FZ_STORE_DEFAULT);
        fz_register_document_handlers(created);
        return created;
    }();
    return ctx;
}

fz_context *threadContext()
{
    thread_local ThreadClone clone;
    return clone.ctx;
}

QString takeError(fz_context *ctx)
{
    int code = 0;
    return QString::fromUtf8(fz_convert_error(ctx, &code));
}
