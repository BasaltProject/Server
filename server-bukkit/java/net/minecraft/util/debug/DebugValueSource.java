// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.debug;

import net.minecraft.server.level.ServerLevel;
public interface DebugValueSource {
    default void registerDebugValues(ServerLevel level, Registration registration) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.util.debug.DebugValueSource#registerDebugValues");
    }
    public interface Registration {
        <T> void register(DebugSubscription<T> subscription, ValueGetter<T> getter);
    }
    public interface ValueGetter<T> {
        T get();
    }
}
