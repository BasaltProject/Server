// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands;

public interface CommandResultCallback {
    CommandResultCallback EMPTY = new CommandResultCallback() {
        @Override
        public void onResult(boolean success, int result) {}

        @Override
        public String toString() {
            return "<empty>";
        }
    };

    void onResult(boolean success, int result);

    default void onSuccess(int result) {
        onResult(true, result);
    }

    default void onFailure() {
        onResult(false, 0);
    }

    static CommandResultCallback chain(CommandResultCallback first, CommandResultCallback second) {
        if (first == EMPTY) {
            return second;
        }
        if (second == EMPTY) {
            return first;
        }
        return (success, result) -> {
            first.onResult(success, result);
            second.onResult(success, result);
        };
    }
}
