package org.basaltmc.bukkit.facade;

import java.io.DataInput;
import java.io.IOException;
import net.minecraft.nbt.NbtAccounter;
import net.minecraft.nbt.StreamTagVisitor;
import net.minecraft.nbt.Tag;
import net.minecraft.nbt.TagType;

public final class NbtSupport {
    private NbtSupport() {}

    @FunctionalInterface
    public interface Loader<T extends Tag> {
        T load(DataInput input, NbtAccounter accounter) throws IOException;
    }

    public static <T extends Tag> TagType<T> type(String name, String pretty, Loader<T> loader) {
        return new TagType<>() {
            @Override
            public T load(DataInput input, NbtAccounter accounter) throws IOException {
                return loader.load(input, accounter);
            }

            @Override
            public StreamTagVisitor.ValueResult parse(DataInput input, StreamTagVisitor output, NbtAccounter accounter)
                throws IOException {
                return load(input, accounter).accept(output);
            }

            @Override
            public void skip(DataInput input, int count, NbtAccounter accounter) throws IOException {
                for (int i = 0; i < count; i++) {
                    load(input, accounter);
                }
            }

            @Override
            public void skip(DataInput input, NbtAccounter accounter) throws IOException {
                load(input, accounter);
            }

            @Override
            public String getName() {
                return name;
            }

            @Override
            public String getPrettyName() {
                return pretty;
            }
        };
    }
}
