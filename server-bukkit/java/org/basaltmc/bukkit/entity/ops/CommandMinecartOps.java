package org.basaltmc.bukkit.entity.ops;

import net.kyori.adventure.text.Component;
import net.kyori.adventure.text.serializer.gson.GsonComponentSerializer;
import org.basaltmc.bukkit.entity.Cored;
import org.bukkit.entity.minecart.CommandMinecart;

public interface CommandMinecartOps extends CommandMinecart, Cored {
    default String getCommand() {
        String command = core().nbtText("Command");
        return command == null ? "" : command;
    }

    default void setCommand(String command) {
        core().nbtSet("Command", "string", command == null ? "" : command);
    }

    default void setName(String name) {
        setCustomName(name == null || name.isEmpty() ? null : name);
    }

    default Component lastOutput() {
        String json = core().nbtText("LastOutput");
        return json == null || json.isEmpty() ? null : GsonComponentSerializer.gson().deserialize(json);
    }

    default void lastOutput(Component output) {
        if (output == null) {
            core().nbtRemove("LastOutput");
        } else {
            core().nbtSet("LastOutput", "string", GsonComponentSerializer.gson().serialize(output));
        }
    }

    default int getSuccessCount() {
        return core().nbtInt("SuccessCount");
    }

    default void setSuccessCount(int count) {
        core().nbtSet("SuccessCount", "int", count);
    }
}
