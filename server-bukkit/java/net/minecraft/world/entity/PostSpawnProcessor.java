// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;

public interface PostSpawnProcessor<T extends Entity> {
    void apply(T target);

    default PostSpawnProcessor<T> andThen(PostSpawnProcessor<? super T> after) {
        return target -> {
            apply(target);
            after.apply(target);
        };
    }

    static <T extends Entity> PostSpawnProcessor<T> nop() {
        return target -> {
        };
    }
}
