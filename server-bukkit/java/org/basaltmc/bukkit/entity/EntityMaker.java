package org.basaltmc.bukkit.entity;

import org.bukkit.entity.Entity;

public interface EntityMaker {
    Entity make(String type, EntityCore core);
}
