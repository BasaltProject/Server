package org.basaltmc.bukkit.facade;
import java.lang.reflect.Constructor;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
public final class VariantHolders {
    private VariantHolders() {
    }
    public static <T> Holder<T> of(ResourceKey<? extends Registry<T>> registry, Class<T> type, org.bukkit.Keyed variant) {
        ResourceKey<T> key = ResourceKey.create(registry, Identifier.parse(variant.getKey().toString()));
        return new Holder.Reference<>(null, key, instantiate(type));
    }
    private static <T> T instantiate(Class<T> type) {
        for (Constructor<?> constructor : type.getDeclaredConstructors()) {
            Class<?>[] params = constructor.getParameterTypes();
            Object[] args = new Object[params.length];
            for (int i = 0; i < params.length; i++) {
                args[i] = params[i] == boolean.class ? Boolean.FALSE : params[i].isPrimitive() ? (Object) 0 : null;
            }
            try {
                constructor.setAccessible(true);
                return type.cast(constructor.newInstance(args));
            } catch (ReflectiveOperationException | RuntimeException e) {
                continue;
            }
        }
        return null;
    }

    private static java.util.List<org.bukkit.Keyed> ordered(io.papermc.paper.registry.RegistryKey<? extends org.bukkit.Keyed> key) {
        java.util.List<org.bukkit.Keyed> out = new java.util.ArrayList<>();
        io.papermc.paper.registry.RegistryAccess.registryAccess().getRegistry(key).forEach(out::add);
        return out;
    }
    public static int indexOf(io.papermc.paper.registry.RegistryKey<? extends org.bukkit.Keyed> key, Holder<?> holder) {
        String wanted = holder.unwrapKey().map(k -> k.identifier().toString()).orElse("");
        java.util.List<org.bukkit.Keyed> all = ordered(key);
        for (int i = 0; i < all.size(); i++) {
            if (all.get(i).getKey().toString().equals(wanted)) {
                return i;
            }
        }
        throw new IllegalStateException("Unknown variant " + wanted);
    }
    public static <T> Holder<T> at(ResourceKey<? extends Registry<T>> registry, Class<T> type, io.papermc.paper.registry.RegistryKey<? extends org.bukkit.Keyed> key, int index) {
        return of(registry, type, ordered(key).get(index));
    }
}
