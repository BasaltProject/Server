package org.basaltmc.bukkit.facade;

import com.mojang.serialization.MapCodec;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.world.level.storage.ValueInput;
import net.minecraft.world.level.storage.ValueOutput;

public final class ValueIo {
    private static final MapCodec<CompoundTag> kCompound = MapCodec.assumeMapUnsafe(CompoundTag.CODEC);

    private ValueIo() {}

    @SuppressWarnings("deprecation")
    public static CompoundTag read(ValueInput input) {
        return input.read(kCompound).orElseGet(CompoundTag::new);
    }

    @SuppressWarnings("deprecation")
    public static void write(ValueOutput output, CompoundTag tag) {
        output.store(kCompound, tag);
    }
}
