// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.eventlog;

import com.google.gson.Gson;
import com.google.gson.JsonElement;
import com.mojang.serialization.Codec;
import com.mojang.serialization.JsonOps;
import java.io.Closeable;
import java.io.IOException;
import java.io.Writer;
import java.nio.channels.Channels;
import java.nio.channels.FileChannel;
import java.nio.charset.StandardCharsets;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.concurrent.atomic.AtomicInteger;

public class JsonEventLog<T> implements Closeable {
    private static final Gson kGson = new Gson();
    private final Codec<T> codec;
    private final FileChannel channel;
    private final AtomicInteger references = new AtomicInteger(1);

    public JsonEventLog(Codec<T> codec, FileChannel channel) {
        this.codec = codec;
        this.channel = channel;
    }

    public static <T> JsonEventLog<T> open(Codec<T> codec, Path path) throws IOException {
        return new JsonEventLog<>(codec, FileChannel.open(path, StandardOpenOption.WRITE, StandardOpenOption.READ, StandardOpenOption.CREATE));
    }

    public void write(T event) throws IOException {
        JsonElement json = codec.encodeStart(JsonOps.INSTANCE, event).getOrThrow(IOException::new);
        channel.position(channel.size());
        Writer writer = Channels.newWriter(channel, StandardCharsets.UTF_8);
        kGson.toJson(json, kGson.newJsonWriter(writer));
        writer.write('\n');
        writer.flush();
    }

    public JsonEventLogReader<T> openReader() throws IOException {
        if (references.get() <= 0) {
            throw new IOException("Event log has already been closed");
        }
        references.incrementAndGet();
        JsonEventLogReader<T> inner = JsonEventLogReader.create(codec, Channels.newReader(channel, StandardCharsets.UTF_8));
        return new JsonEventLogReader<>() {
            private volatile long position;

            @Override
            public T next() throws IOException {
                try {
                    channel.position(position);
                    return inner.next();
                } finally {
                    position = channel.position();
                }
            }

            @Override
            public void close() throws IOException {
                release();
            }
        };
    }

    @Override
    public void close() throws IOException {
        release();
    }

    private void release() throws IOException {
        if (references.decrementAndGet() <= 0) {
            channel.close();
        }
    }
}
