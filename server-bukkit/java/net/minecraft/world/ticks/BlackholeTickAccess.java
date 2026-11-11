// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.ticks;
import net.minecraft.core.BlockPos;
public class BlackholeTickAccess {
    private static final TickContainerAccess<Object> kContainer = new TickContainerAccess<>() {
        public void schedule(ScheduledTick<Object> tick) {
        }
        public boolean hasScheduledTick(BlockPos pos, Object type) {
            return false;
        }
        public int count() {
            return 0;
        }
    };
    private static final LevelTickAccess<Object> kLevel = new LevelTickAccess<>() {
        public void schedule(ScheduledTick<Object> tick) {
        }
        public boolean hasScheduledTick(BlockPos pos, Object type) {
            return false;
        }
        public boolean willTickThisTick(BlockPos pos, Object type) {
            return false;
        }
        public int count() {
            return 0;
        }
    };
    @SuppressWarnings("unchecked")
    public static <T> TickContainerAccess<T> emptyContainer() {
        return (TickContainerAccess<T>) kContainer;
    }
    @SuppressWarnings("unchecked")
    public static <T> LevelTickAccess<T> emptyLevelList() {
        return (LevelTickAccess<T>) kLevel;
    }
}
