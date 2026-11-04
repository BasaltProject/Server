// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.permissions;

import com.mojang.serialization.Codec;
import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;

public interface PermissionCheck {
    Codec<PermissionCheck> CODEC = Permission.CODEC.xmap(Require::new, check -> ((Require) check).permission());

    boolean check(PermissionSet source);

    MapCodec<? extends PermissionCheck> codec();

    class AlwaysPass implements PermissionCheck {
        public static final AlwaysPass INSTANCE = new AlwaysPass();
        public static final MapCodec<AlwaysPass> MAP_CODEC = MapCodec.unit(INSTANCE);

        private AlwaysPass() {}

        @Override
        public boolean check(PermissionSet source) {
            return true;
        }

        @Override
        public MapCodec<AlwaysPass> codec() {
            return MAP_CODEC;
        }
    }

    record Require(Permission permission) implements PermissionCheck {
        public static final MapCodec<Require> MAP_CODEC = RecordCodecBuilder.mapCodec(instance -> instance.group(
            Permission.CODEC.fieldOf("permission").forGetter(Require::permission)).apply(instance, Require::new));

        @Override
        public MapCodec<Require> codec() {
            return MAP_CODEC;
        }

        @Override
        public boolean check(PermissionSet source) {
            return source.hasPermission(permission);
        }
    }
}
