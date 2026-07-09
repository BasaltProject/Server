package org.basaltmc.bukkit.entity.ops;

import org.basaltmc.bukkit.entity.Cored;

public interface CommandMinecartOps extends org.bukkit.entity.minecart.CommandMinecart, Cored {
    default String getCommand() {
        String command = core().nbtText("Command");
        return command == null ? "" : command;
    }

    default void setCommand(String command) {
        core().nbtSet("Command", "string", command == null ? "" : command);
    }

    default int getSuccessCount() {
        return core().nbtInt("SuccessCount");
    }

    default void setSuccessCount(int count) {
        core().nbtSet("SuccessCount", "int", count);
    }

    default net.kyori.adventure.text.Component lastOutput() {
        Object raw = EntityNbt.compound(core(), "").get("LastOutput");
        return raw == null ? null : org.basaltmc.bukkit.item.Texts.fromNbt(raw);
    }

    default void lastOutput(net.kyori.adventure.text.Component output) {
        if (output == null) {
            core().nbtRemove("LastOutput");
        } else {
            EntityNbt.put(core(), "LastOutput", org.basaltmc.bukkit.item.Texts.toNbt(output));
        }
    }

    @Deprecated
    default void setName(String name) {
        if (name == null) {
            core().nbtRemove("CustomName");
        } else {
            EntityNbt.put(core(), "CustomName", org.basaltmc.bukkit.item.Texts.legacyToNbt(name));
        }
    }
}
