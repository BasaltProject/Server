// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.permissions;

import java.util.function.Predicate;

public record PermissionProviderCheck<T extends PermissionSetSupplier>(PermissionCheck test) implements Predicate<T> {
    @Override
    public boolean test(T t) {
        return test.check(t.permissions());
    }
}
