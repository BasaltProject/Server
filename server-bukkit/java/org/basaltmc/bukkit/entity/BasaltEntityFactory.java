package org.basaltmc.bukkit.entity;

import org.bukkit.entity.EntityFactory;
import org.bukkit.entity.EntitySnapshot;

public final class BasaltEntityFactory implements EntityFactory {
    public static final BasaltEntityFactory instance = new BasaltEntityFactory();

    private BasaltEntityFactory() {}

    @Override
    public EntitySnapshot createEntitySnapshot(String snbt) throws IllegalArgumentException {
        return BasaltEntitySnapshot.parse(snbt);
    }
}
