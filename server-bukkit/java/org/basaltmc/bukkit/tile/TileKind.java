package org.basaltmc.bukkit.tile;

import java.lang.reflect.Method;
import org.bukkit.block.BlockState;

interface TileKind {
    Class<? extends BlockState> face();

    /** The result of the method, or {@link Tile#kNone} when this kind does not know it. */
    Object call(Tile tile, Method method, String name, Object[] args) throws Throwable;
}
