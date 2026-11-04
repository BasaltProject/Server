// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.nbt;
import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.DataInput;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.util.zip.GZIPInputStream;
import java.util.zip.GZIPOutputStream;
import java.io.DataOutput;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.nio.file.Path;
import net.minecraft.util.DelegateDataOutput;
public class NbtIo {
    public static CompoundTag readCompressed(Path file, NbtAccounter accounter) throws IOException {
        try (InputStream in = java.nio.file.Files.newInputStream(file)) {
            return readCompressed(in, accounter);
        }
    }
    public static CompoundTag readCompressed(InputStream in, NbtAccounter accounter) throws IOException {
        try (DataInputStream data = new DataInputStream(new BufferedInputStream(new GZIPInputStream(in)))) {
            return read(data, accounter);
        }
    }
    public static void parseCompressed(Path file, StreamTagVisitor output, NbtAccounter accounter) throws IOException {
        try (InputStream in = java.nio.file.Files.newInputStream(file)) {
            parseCompressed(in, output, accounter);
        }
    }
    public static void parseCompressed(InputStream in, StreamTagVisitor output, NbtAccounter accounter) throws IOException {
        try (DataInputStream data = new DataInputStream(new BufferedInputStream(new GZIPInputStream(in)))) {
            parse(data, output, accounter);
        }
    }
    public static void writeCompressed(CompoundTag tag, Path file) throws IOException {
        try (OutputStream out = java.nio.file.Files.newOutputStream(file)) {
            writeCompressed(tag, out);
        }
    }
    public static void writeCompressed(CompoundTag tag, OutputStream out) throws IOException {
        try (DataOutputStream data = new DataOutputStream(new BufferedOutputStream(new GZIPOutputStream(out)))) {
            write(tag, data);
        }
    }
    public static void write(CompoundTag tag, Path file) throws IOException {
        try (DataOutputStream data = new DataOutputStream(new BufferedOutputStream(java.nio.file.Files.newOutputStream(file)))) {
            write(tag, data);
        }
    }
    public static CompoundTag read(Path file) throws IOException {
        try (DataInputStream data = new DataInputStream(new BufferedInputStream(java.nio.file.Files.newInputStream(file)))) {
            return read(data, NbtAccounter.unlimitedHeap());
        }
    }
    public static CompoundTag read(DataInput input) throws IOException {
        return read(input, NbtAccounter.unlimitedHeap());
    }
    public static CompoundTag read(DataInput input, NbtAccounter accounter) throws IOException {
        Tag tag = readUnnamedTag(input, accounter);
        if (tag instanceof CompoundTag compound) {
            return compound;
        }
        throw new IOException("Root tag must be a named compound tag");
    }
    public static void write(CompoundTag tag, DataOutput output) throws IOException {
        writeUnnamedTagWithFallback(tag, output);
    }
    public static void parse(DataInput input, StreamTagVisitor output, NbtAccounter accounter) throws IOException {
        TagType<?> type = TagTypes.getType(input.readByte());
        if (type == EndTag.TYPE) {
            if (output.visitRootEntry(EndTag.TYPE) == StreamTagVisitor.ValueResult.CONTINUE) {
        output.visitEnd();
            }
            return;
        }
        StringTag.skipString(input);
        type.parseRoot(input, output, accounter);
    }
    public static Tag readAnyTag(DataInput input, NbtAccounter accounter) throws IOException {
        byte id = input.readByte();
        if (id == Tag.TAG_END) {
            return EndTag.INSTANCE;
        }
        return TagTypes.getType(id).load(input, accounter);
    }
    public static void writeAnyTag(Tag tag, DataOutput output) throws IOException {
        output.writeByte(tag.getId());
        if (tag.getId() != Tag.TAG_END) {
            tag.write(output);
        }
    }
    public static void writeUnnamedTag(Tag tag, DataOutput output) throws IOException {
        output.writeByte(tag.getId());
        if (tag.getId() != Tag.TAG_END) {
            output.writeUTF("");
            tag.write(output);
        }
    }
    public static void writeUnnamedTagWithFallback(Tag tag, DataOutput output) throws IOException {
        writeUnnamedTag(tag, new StringFallbackDataOutput(output));
    }
    public static Tag readUnnamedTag(DataInput input, NbtAccounter accounter) throws IOException {
        byte id = input.readByte();
        if (id == Tag.TAG_END) {
            return EndTag.INSTANCE;
        }
        StringTag.skipString(input);
        return TagTypes.getType(id).load(input, accounter);
    }
    public static class StringFallbackDataOutput extends DelegateDataOutput {
        public StringFallbackDataOutput(DataOutput parent) {
        super(parent);
        }
        public void writeUTF(String s) throws IOException {
        byte[] encoded = s.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        if (encoded.length > 65535) {
            super.writeUTF("");
        } else {
            super.writeUTF(s);
        }
        }
    }
}
