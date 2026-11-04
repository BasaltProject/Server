// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands;

import net.minecraft.network.chat.Component;

public interface CommandSource {
    CommandSource NULL = new CommandSource() {
        @Override
        public void sendSystemMessage(Component message) {}

        @Override
        public boolean acceptsSuccess() {
            return false;
        }

        @Override
        public boolean acceptsFailure() {
            return false;
        }

        @Override
        public boolean shouldInformAdmins() {
            return false;
        }
    };

    default void sendSystemMessage(Component message) {}

    default boolean acceptsSuccess() {
        return true;
    }

    default boolean acceptsFailure() {
        return true;
    }

    default boolean shouldInformAdmins() {
        return true;
    }

    default boolean alwaysAccepts() {
        return false;
    }
}
