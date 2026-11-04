// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.permissions;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.minecraft.resources.Identifier;

public interface Permission {
    Codec<Permission> FULL_CODEC = Atom.MAP_CODEC.codec().xmap(atom -> (Permission) atom, permission -> (Atom) permission);
    Codec<Permission> CODEC = Identifier.CODEC.xmap(id -> (Permission) new Atom(id), permission -> ((Atom) permission).id());

    MapCodec<? extends Permission> codec();

    record Atom(Identifier id) implements Permission {
        public static final MapCodec<Atom> MAP_CODEC = RecordCodecBuilder.mapCodec(instance -> instance.group(
            Identifier.CODEC.fieldOf("id").forGetter(Atom::id)).apply(instance, Atom::new));

        @Override
        public MapCodec<Atom> codec() {
            return MAP_CODEC;
        }

        public static Atom create(String name) {
            return create(Identifier.withDefaultNamespace(name));
        }

        public static Atom create(Identifier id) {
            return new Atom(id);
        }
    }

    record HasCommandLevel(PermissionLevel level) implements Permission {
        public static final MapCodec<HasCommandLevel> MAP_CODEC = RecordCodecBuilder.mapCodec(instance -> instance.group(
            PermissionLevel.CODEC.fieldOf("level").forGetter(HasCommandLevel::level)).apply(instance, HasCommandLevel::new));

        @Override
        public MapCodec<HasCommandLevel> codec() {
            return MAP_CODEC;
        }
    }
}
