package org.basaltmc.bukkit;

import io.papermc.paper.ServerBuildInfo;
import java.time.Instant;
import java.util.Optional;
import java.util.OptionalInt;
import net.kyori.adventure.key.Key;

public final class BasaltBuildInfo implements ServerBuildInfo {
    private static final Key kBrand = Key.key("basalt", "basalt");

    @Override
    public Key brandId() {
        return kBrand;
    }

    @Override
    public boolean isBrandCompatible(Key brandId) {
        return brandId.equals(kBrand) || brandId.equals(BRAND_PAPER_ID);
    }

    @Override
    public String brandName() {
        return "Basalt";
    }

    @Override
    public String minecraftVersionId() {
        return BasaltServer.kMinecraftVersion;
    }

    @Override
    public String minecraftVersionName() {
        return BasaltServer.kMinecraftVersion;
    }

    @Override
    public OptionalInt buildNumber() {
        return OptionalInt.empty();
    }

    @Override
    public Instant buildTime() {
        return Instant.EPOCH;
    }

    @Override
    public Optional<String> gitBranch() {
        return Optional.empty();
    }

    @Override
    public Optional<String> gitCommit() {
        return Optional.empty();
    }

    @Override
    public String asString(StringRepresentation representation) {
        return "Basalt " + BasaltServer.kMinecraftVersion;
    }
}
