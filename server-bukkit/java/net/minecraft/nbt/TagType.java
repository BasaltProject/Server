// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;
import java.io.DataInput;
import java.io.IOException;
public interface TagType<T extends Tag> {
    T load(DataInput input, NbtAccounter accounter) throws IOException;
    StreamTagVisitor.ValueResult parse(DataInput input, StreamTagVisitor output, NbtAccounter accounter) throws IOException;
    default void parseRoot(DataInput input, StreamTagVisitor output, NbtAccounter accounter) throws IOException {
        StreamTagVisitor.ValueResult result = output.visitRootEntry(this);
        if (result == StreamTagVisitor.ValueResult.CONTINUE) {
            parse(input, output, accounter);
        }
    }
    void skip(DataInput input, int count, NbtAccounter accounter) throws IOException;
    void skip(DataInput input, NbtAccounter accounter) throws IOException;
    String getName();
    String getPrettyName();
    static TagType<EndTag> createInvalid(int id) {
        return new TagType<>() {
            private IOException error() {
        return new IOException("Invalid tag id: " + id);
            }

            @Override
            public EndTag load(DataInput input, NbtAccounter accounter) throws IOException {
        throw error();
            }

            @Override
            public StreamTagVisitor.ValueResult parse(DataInput input, StreamTagVisitor output, NbtAccounter accounter)
        throws IOException {
        throw error();
            }

            @Override
            public void skip(DataInput input, int count, NbtAccounter accounter) throws IOException {
        throw error();
            }

            @Override
            public void skip(DataInput input, NbtAccounter accounter) throws IOException {
        throw error();
            }

            @Override
            public String getName() {
        return "INVALID[" + id + "]";
            }

            @Override
            public String getPrettyName() {
        return "UNKNOWN_" + id;
            }
        };
    }
    public interface StaticSize<T extends Tag> extends TagType<T> {
        default void skip(DataInput input, NbtAccounter accounter) throws IOException {
        input.skipBytes(size());
        }
        default void skip(DataInput input, int count, NbtAccounter accounter) throws IOException {
            input.skipBytes(size() * count);
        }
        int size();
    }
    public interface VariableSize<T extends Tag> extends TagType<T> {
        default void skip(DataInput input, int count, NbtAccounter accounter) throws IOException {
            for (int i = 0; i < count; i++) {
                skip(input, accounter);
            }
        }
    }
}
