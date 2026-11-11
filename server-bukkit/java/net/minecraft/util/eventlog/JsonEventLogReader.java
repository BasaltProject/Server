// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.eventlog;

import com.google.gson.JsonElement;
import com.google.gson.JsonParseException;
import com.google.gson.JsonParser;
import com.google.gson.Strictness;
import com.google.gson.stream.JsonReader;
import com.mojang.serialization.Codec;
import com.mojang.serialization.JsonOps;
import java.io.Closeable;
import java.io.EOFException;
import java.io.IOException;
import java.io.Reader;

public interface JsonEventLogReader<T> extends Closeable {
    static <T> JsonEventLogReader<T> create(Codec<T> codec, Reader reader) {
        JsonReader json = new JsonReader(reader);
        json.setStrictness(Strictness.LENIENT);
        return new JsonEventLogReader<>() {
            @Override
            public T next() throws IOException {
                try {
                    if (!json.hasNext()) {
                        return null;
                    }
                    JsonElement element = JsonParser.parseReader(json);
                    return codec.parse(JsonOps.INSTANCE, element).getOrThrow(IOException::new);
                } catch (JsonParseException e) {
                    throw new IOException(e);
                } catch (EOFException e) {
                    return null;
                }
            }

            @Override
            public void close() throws IOException {
                json.close();
            }
        };
    }

    T next() throws IOException;
}
