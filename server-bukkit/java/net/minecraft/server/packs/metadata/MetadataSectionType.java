// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.packs.metadata;
import com.mojang.serialization.Codec;
import java.util.Optional;
public record MetadataSectionType<T>(String name, Codec<T> codec) {
    public WithValue<T> withValue(T value) {
        return new WithValue<>(this, value);
    }
    public static record WithValue<T>(MetadataSectionType<T> type, T value) {
        public <U> Optional<U> unwrapToType(MetadataSectionType<U> type) {
            return type == this.type ? Optional.of((U) value) : Optional.empty();
        }
    }
}
