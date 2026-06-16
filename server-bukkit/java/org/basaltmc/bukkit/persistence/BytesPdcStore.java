package org.basaltmc.bukkit.persistence;

import java.util.function.Consumer;
import java.util.function.Supplier;
import net.minecraft.nbt.CompoundTag;
import org.basaltmc.bukkit.facade.NbtCodec;

/** A store over a named NBT compound held as bytes by the server; an empty or missing blob is an empty container. */
public final class BytesPdcStore implements PdcStore {
    private final Supplier<byte[]> reader;
    private final Consumer<byte[]> writer;

    public BytesPdcStore(Supplier<byte[]> reader, Consumer<byte[]> writer) {
        this.reader = reader;
        this.writer = writer;
    }

    @Override
    public CompoundTag load() {
        CompoundTag found = NbtCodec.fromBytes(reader.get());
        return found == null ? new CompoundTag() : found;
    }

    @Override
    public void save(CompoundTag tag) {
        writer.accept(NbtCodec.toBytes(tag));
    }
}
