package net.minecraft;

public class SharedConstants {
    private static final WorldVersion kVersion = new WorldVersion() {
        private final net.minecraft.world.level.storage.DataVersion data =
            new net.minecraft.world.level.storage.DataVersion(5023, "main");

        @Override
        public net.minecraft.world.level.storage.DataVersion dataVersion() {
            return data;
        }

        @Override
        public String id() {
            return "26.3";
        }

        @Override
        public String name() {
            return "26.3";
        }

        @Override
        public int protocolVersion() {
            return 777;
        }
    };

    private SharedConstants() {}

    public static WorldVersion getCurrentVersion() {
        return kVersion;
    }
}
