// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity;
public interface UpdateInterval {
    UpdateInterval NEVER = new UpdateInterval() {
        public boolean test(int tick) {
            return false;
        }

        public int nextInterval(int tick) {
            return 0;
        }
    };
    boolean test(int tick);
    int nextInterval(int tick);
    static UpdateInterval periodic(int period) {
        return new UpdateInterval() {
            public boolean test(int tick) {
                return tick % period == 0;
            }

            public int nextInterval(int tick) {
                return (tick / period + 1) * period;
            }
        };
    }
}
