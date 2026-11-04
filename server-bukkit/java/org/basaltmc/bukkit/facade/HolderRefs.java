package org.basaltmc.bukkit.facade;

import java.util.Locale;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;
import net.minecraft.core.Holder;
import net.minecraft.core.Registry;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;

/** Resolves a value class and an id to a stable holder, bound when a native-backed registry knows the entry. */
public final class HolderRefs {
    private static final ConcurrentHashMap<String, Holder<?>> kStandalone = new ConcurrentHashMap<>();

    private HolderRefs() {}

    public static ResourceKey<? extends Registry<Object>> registryKey(Class<?> valueClass) {
        StringBuilder name = new StringBuilder();
        for (char c : valueClass.getSimpleName().toCharArray()) {
            if (Character.isUpperCase(c) && name.length() > 0) {
                name.append('_');
            }
            name.append(Character.toLowerCase(c));
        }
        return ResourceKey.createRegistryKey(Identifier.withDefaultNamespace(name.toString().toLowerCase(Locale.ROOT)));
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    private static Registry<?> registryOf(Class<?> valueClass) {
        if (net.minecraft.world.level.block.Block.class.isAssignableFrom(valueClass)) {
            return BuiltInRegistries.BLOCK;
        }
        if (net.minecraft.world.item.Item.class.isAssignableFrom(valueClass)) {
            return BuiltInRegistries.ITEM;
        }
        if (net.minecraft.world.entity.EntityType.class.isAssignableFrom(valueClass)) {
            return BuiltInRegistries.ENTITY_TYPE;
        }
        if (net.minecraft.world.effect.MobEffect.class.isAssignableFrom(valueClass)) {
            return BuiltInRegistries.MOB_EFFECT;
        }
        if (net.minecraft.world.entity.ai.attributes.Attribute.class.isAssignableFrom(valueClass)) {
            return BuiltInRegistries.ATTRIBUTE;
        }
        return null;
    }

    private static final java.util.Map<String, Class<?>> kKnown = java.util.Map.of("block", net.minecraft.world.level.block.Block.class,
        "item", net.minecraft.world.item.Item.class, "entity_type", net.minecraft.world.entity.EntityType.class,
        "mob_effect", net.minecraft.world.effect.MobEffect.class, "attribute", net.minecraft.world.entity.ai.attributes.Attribute.class,
        "enchantment", net.minecraft.world.item.enchantment.Enchantment.class, "potion", net.minecraft.world.item.alchemy.Potion.class);

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static Holder<?> ofKey(ResourceKey<? extends Registry<?>> registry, Identifier id) {
        Class<?> known = "minecraft".equals(registry.identifier().getNamespace()) ? kKnown.get(registry.identifier().getPath()) : null;
        if (known != null) {
            return of(known, id);
        }
        return kStandalone.computeIfAbsent(registry.identifier() + "|" + id,
            key -> Holder.Reference.createStandAlone(null, ResourceKey.create((ResourceKey) registry, id)));
    }

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static <T> Holder<T> of(Class<?> valueClass, Identifier id) {
        Registry<?> registry;
        try {
            registry = registryOf(valueClass);
        } catch (ExceptionInInitializerError | NoClassDefFoundError failure) {
            registry = null;
        }
        if (registry != null) {
            Optional<? extends Holder<?>> found = ((Registry) registry).get(id);
            if (found.isPresent()) {
                return (Holder<T>) found.get();
            }
        }
        String cacheKey = valueClass.getName() + "|" + id;
        if (valueClass == net.minecraft.world.item.enchantment.Enchantment.class) {
            return (Holder<T>) kStandalone.computeIfAbsent(cacheKey, key -> new net.minecraft.core.LazyHolder(
                ResourceKey.create((ResourceKey) registryKey(valueClass), id), () -> EnchantmentFactory.build(id)));
        }
        if (valueClass == net.minecraft.world.item.alchemy.Potion.class) {
            return (Holder<T>) kStandalone.computeIfAbsent(cacheKey, key -> new net.minecraft.core.LazyHolder(
                ResourceKey.create((ResourceKey) registryKey(valueClass), id), () -> PotionFactory.build(id)));
        }
        return (Holder<T>) kStandalone.computeIfAbsent(cacheKey, key -> Holder.Reference.createStandAlone(null,
            ResourceKey.create((ResourceKey) registryKey(valueClass), id)));
    }
}
