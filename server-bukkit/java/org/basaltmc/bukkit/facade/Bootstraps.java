package org.basaltmc.bukkit.facade;

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import net.minecraft.core.Registry;
import net.minecraft.data.worldgen.BootstrapContext;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.MinecraftServer;

/** Replays the native definitions of a registry into a datagen bootstrap context, keyed by the constants of a class. */
public final class Bootstraps {
    private Bootstraps() {}

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static <T> void replay(BootstrapContext<T> context, ResourceKey<? extends Registry<?>> registryKey, Class<?> constants) {
        MinecraftServer server = MinecraftServer.getServer();
        if (server == null) {
            throw new IllegalStateException("The native registries are not available before the server starts");
        }
        Registry<T> registry = (Registry<T>) server.registryAccess().lookupOrThrow((ResourceKey) registryKey);
        for (Field field : constants.getDeclaredFields()) {
            if (!Modifier.isStatic(field.getModifiers()) || field.getType() != ResourceKey.class) {
                continue;
            }
            try {
                field.setAccessible(true);
                ResourceKey<T> key = (ResourceKey<T>) field.get(null);
                registry.get(key).ifPresent(holder -> context.register(key, holder.value()));
            } catch (IllegalAccessException failure) {
                throw new IllegalStateException(failure);
            }
        }
    }
}
