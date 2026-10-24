package org.basaltmc.bukkit.dialog;

import io.papermc.paper.dialog.Dialog;
import io.papermc.paper.registry.RegistryBuilderFactory;
import io.papermc.paper.registry.TypedKey;
import io.papermc.paper.registry.data.dialog.DialogBase;
import io.papermc.paper.registry.data.dialog.DialogRegistryEntry;
import io.papermc.paper.registry.data.dialog.type.DialogType;
import io.papermc.paper.registry.set.RegistryValueSet;
import io.papermc.paper.registry.set.RegistryValueSetBuilder;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import org.basaltmc.bukkit.gen.InlinedRegistryBuilderProviderBase;

/** The entries a plugin makes on the spot instead of reading them from the registry. */
public final class BasaltInlineRegistries extends InlinedRegistryBuilderProviderBase {
    @Override
    public Dialog createDialog(Consumer<RegistryBuilderFactory<Dialog, ? extends DialogRegistryEntry.Builder>> value) {
        Builder builder = new Builder();
        value.accept(new RegistryBuilderFactory<Dialog, Builder>() {
            @Override
            public Builder empty() {
                return builder;
            }

            @Override
            public Builder copyFrom(TypedKey<Dialog> key) {
                throw new UnsupportedOperationException("A dialog is built from nothing");
            }
        });
        return builder.build();
    }

    private static final class Builder implements DialogRegistryEntry.Builder {
        private DialogBase base;
        private DialogType type;

        @Override
        public RegistryValueSetBuilder<Dialog, DialogRegistryEntry.Builder> registryValueSet() {
            List<Dialog> made = new ArrayList<>();
            return new RegistryValueSetBuilder<>() {
                @Override
                public RegistryValueSetBuilder<Dialog, DialogRegistryEntry.Builder> add(
                        Consumer<RegistryBuilderFactory<Dialog, ? extends DialogRegistryEntry.Builder>> entry) {
                    made.add(new BasaltInlineRegistries().createDialog(entry));
                    return this;
                }

                @Override
                public RegistryValueSet<Dialog> build() {
                    return io.papermc.paper.registry.set.RegistrySet.valueSet(
                        io.papermc.paper.registry.RegistryKey.DIALOG, List.copyOf(made));
                }
            };
        }

        @Override
        public DialogRegistryEntry.Builder base(DialogBase dialogBase) {
            this.base = java.util.Objects.requireNonNull(dialogBase, "dialogBase");
            return this;
        }

        @Override
        public DialogRegistryEntry.Builder type(DialogType dialogType) {
            this.type = java.util.Objects.requireNonNull(dialogType, "dialogType");
            return this;
        }

        @Override
        public DialogBase base() {
            if (base == null) {
                throw new IllegalStateException("The dialog base has not been set");
            }
            return base;
        }

        @Override
        public DialogType type() {
            if (type == null) {
                throw new IllegalStateException("The dialog type has not been set");
            }
            return type;
        }

        Dialog build() {
            return new BasaltDialog(base(), type());
        }
    }
}
