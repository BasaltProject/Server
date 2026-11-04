// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world;

import java.util.ArrayList;
import java.util.List;
import java.util.function.UnaryOperator;
import net.minecraft.resources.Identifier;
import net.minecraft.world.level.saveddata.SavedData;
import net.minecraft.world.level.saveddata.SavedDataType;
import org.basaltmc.bukkit.LeftoverNatives;

/** The stopwatches of the server, kept by the native `/stopwatch` command. */
public class Stopwatches extends SavedData {
    public static final SavedDataType<Stopwatches> TYPE = null;

    public Stopwatches() {
    }

    public Stopwatch get(Identifier id) {
        double seconds = LeftoverNatives.stopwatch("elapsed", id.toString());
        return seconds < 0 ? null : new Stopwatch(currentTime(), Math.round(seconds * 1000.0));
    }

    public boolean add(Identifier id, Stopwatch stopwatch) {
        return LeftoverNatives.stopwatch("create", id.toString()) >= 0;
    }

    public boolean update(Identifier id, UnaryOperator<Stopwatch> update) {
        Stopwatch current = get(id);
        if (current == null) {
            return false;
        }
        Stopwatch changed = update.apply(current);
        if (changed.accumulatedElapsedTime() == 0 && changed.creationTime() >= current.creationTime()) {
            LeftoverNatives.stopwatch("restart", id.toString());
        }
        return true;
    }

    public boolean remove(Identifier id) {
        return LeftoverNatives.stopwatch("remove", id.toString()) >= 0;
    }

    public boolean isDirty() {
        return false;
    }

    public List<Identifier> ids() {
        List<Identifier> out = new ArrayList<>();
        for (String name : LeftoverNatives.stopwatchIds()) {
            out.add(Identifier.parse(name));
        }
        return out;
    }

    public static long currentTime() {
        return System.currentTimeMillis();
    }
}
