package org.basaltmc.bukkit.tile;

import java.lang.reflect.Method;
import org.bukkit.block.BlockState;

/** One family of tile states: the interfaces its proxy implements and how their methods read and write the data. */
interface TileKind {
    Class<? extends BlockState> face();

    /** The result of the method, or {@link Tile#kNone} when this kind does not know it. */
    Object call(Tile tile, Method method, String name, Object[] args) throws Throwable;
}
