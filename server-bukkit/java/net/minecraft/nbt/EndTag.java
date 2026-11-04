// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;

import java.io.DataOutput;
import java.io.IOException;

public final class EndTag implements Tag {
    public static final EndTag INSTANCE = new EndTag();

    private EndTag() {}

    @Override
    public byte getId() {
        return TAG_END;
    }

    @Override
    public void write(DataOutput output) {}

    @Override
    public EndTag copy() {
        return this;
    }

    @Override
    public String toString() {
        return new StringTagVisitor().visit(this);
    }
    public static final TagType<EndTag> TYPE = new TagType<>() {
        @Override
        public EndTag load(java.io.DataInput input, NbtAccounter accounter) {
            accounter.accountBytes(8);
            return INSTANCE;
        }

        @Override
        public StreamTagVisitor.ValueResult parse(java.io.DataInput input, StreamTagVisitor output,
            NbtAccounter accounter) {
            return output.visitEnd();
        }

        @Override
        public void skip(java.io.DataInput input, int count, NbtAccounter accounter) {}

        @Override
        public void skip(java.io.DataInput input, NbtAccounter accounter) {}

        @Override
        public String getName() {
            return "END";
        }

        @Override
        public String getPrettyName() {
            return "TAG_End";
        }
    };
    public int sizeInBytes() {
        return 8;
    }
    public TagType<EndTag> getType() {
        return TYPE;
    }
    public void accept(TagVisitor visitor) {
        visitor.visitEnd(this);
    }
    public StreamTagVisitor.ValueResult accept(StreamTagVisitor visitor) {
        return visitor.visitEnd();
    }
}
