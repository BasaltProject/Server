package net.minecraft;

import net.minecraft.world.level.storage.DataVersion;

public interface WorldVersion {
    DataVersion dataVersion();

    String id();

    String name();

    int protocolVersion();
}
