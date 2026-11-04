package org.basaltmc.bukkit.facade;

import net.minecraft.core.Holder;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.world.damagesource.DamageType;

/** Builds damage type keys and the reference holders that damage sources carry. */
public final class DamageKeys {
    private DamageKeys() {}

    @SuppressWarnings({"unchecked", "rawtypes"})
    public static ResourceKey<DamageType> of(String path) {
        return (ResourceKey<DamageType>) (ResourceKey) ResourceKey.create(Registries.DAMAGE_TYPE, Identifier.withDefaultNamespace(path));
    }

    public static Holder<DamageType> holder(ResourceKey<DamageType> key) {
        return Holder.Reference.createStandAlone(null, key);
    }
}
