// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level;

import java.util.function.Predicate;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.Vec3;

public class ClipBlockStateContext {
    private final Vec3 from;
    private final Vec3 to;
    private final Predicate<BlockState> block;

    protected ClipBlockStateContext() {
        this(Vec3.ZERO, Vec3.ZERO, state -> false);
    }

    public ClipBlockStateContext(Vec3 from, Vec3 to, Predicate<BlockState> block) {
        this.from = from;
        this.to = to;
        this.block = block;
    }

    public Vec3 getTo() {
        return to;
    }

    public Vec3 getFrom() {
        return from;
    }

    public Predicate<BlockState> isTargetBlock() {
        return block;
    }
}
