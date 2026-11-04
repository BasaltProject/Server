// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item.crafting;
import java.util.List;
import java.util.Optional;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.crafting.display.SlotDisplay;
public record SelectableRecipe<T extends Recipe<?>>(SlotDisplay optionDisplay, Optional<RecipeHolder<T>> recipe) {
    public static <T extends Recipe<?>> StreamCodec<RegistryFriendlyByteBuf, SelectableRecipe<T>> noRecipeCodec() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.crafting.SelectableRecipe#noRecipeCodec");
    }
    public static record SingleInputEntry<T extends Recipe<?>>(Ingredient input, SelectableRecipe<T> recipe) {
        public static <T extends Recipe<?>> StreamCodec<RegistryFriendlyByteBuf, SingleInputEntry<T>> noRecipeCodec() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.crafting.SelectableRecipe.SingleInputEntry#noRecipeCodec");
        }
    }
    public static record SingleInputSet<T extends Recipe<?>>(List<SingleInputEntry<T>> entries) {
        public static <T extends Recipe<?>> SingleInputSet<T> empty() {
            return new SingleInputSet<>(List.of());
        }
        public static <T extends Recipe<?>> StreamCodec<RegistryFriendlyByteBuf, SingleInputSet<T>> noRecipeCodec() {
            throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.item.crafting.SelectableRecipe.SingleInputSet#noRecipeCodec");
        }
        public boolean acceptsInput(ItemStack input) {
            for (SingleInputEntry<T> entry : entries) {
                if (entry.input().test(input)) {
                    return true;
                }
            }
            return false;
        }
        public SingleInputSet<T> selectByInput(ItemStack input) {
            List<SingleInputEntry<T>> selected = new java.util.ArrayList<>();
            for (SingleInputEntry<T> entry : entries) {
                if (entry.input().test(input)) {
                    selected.add(entry);
                }
            }
            return new SingleInputSet<>(selected);
        }
        public boolean isEmpty() {
            return entries.isEmpty();
        }
        public int size() {
            return entries.size();
        }
    }
}
