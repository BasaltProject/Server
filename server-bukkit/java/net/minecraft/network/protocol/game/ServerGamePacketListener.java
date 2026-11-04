// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;

import net.minecraft.network.protocol.common.ServerCommonPacketListener;
import net.minecraft.network.protocol.ping.ServerPingPacketListener;
import net.minecraft.network.ConnectionProtocol;
public interface ServerGamePacketListener extends ServerCommonPacketListener, ServerPingPacketListener {
    default ConnectionProtocol protocol() {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#protocol");
    }
    default void handlePunch(ServerboundPunchPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handlePunch");
    }
    default void handleChat(ServerboundChatPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleChat");
    }
    default void handleChatCommand(ServerboundChatCommandPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleChatCommand");
    }
    default void handleSignedChatCommand(ServerboundChatCommandSignedPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleSignedChatCommand");
    }
    default void handleChatAck(ServerboundChatAckPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleChatAck");
    }
    default void handleClientCommand(ServerboundClientCommandPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleClientCommand");
    }
    default void handleContainerButtonClick(ServerboundContainerButtonClickPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleContainerButtonClick");
    }
    default void handleContainerClick(ServerboundContainerClickPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleContainerClick");
    }
    default void handlePlaceRecipe(ServerboundPlaceRecipePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handlePlaceRecipe");
    }
    default void handleContainerClose(ServerboundContainerClosePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleContainerClose");
    }
    default void handleAttack(ServerboundAttackPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleAttack");
    }
    default void handleInteract(ServerboundInteractPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleInteract");
    }
    default void handleSpectatorAction(ServerboundSpectatorActionPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleSpectatorAction");
    }
    default void handleMovePlayer(ServerboundMovePlayerPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleMovePlayer");
    }
    default void handlePlayerAbilities(ServerboundPlayerAbilitiesPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handlePlayerAbilities");
    }
    default void handlePlayerAction(ServerboundPlayerActionPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handlePlayerAction");
    }
    default void handlePlayerCommand(ServerboundPlayerCommandPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handlePlayerCommand");
    }
    default void handlePlayerInput(ServerboundPlayerInputPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handlePlayerInput");
    }
    default void handleSetCarriedItem(ServerboundSetCarriedItemPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleSetCarriedItem");
    }
    default void handleSetCreativeModeSlot(ServerboundSetCreativeModeSlotPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleSetCreativeModeSlot");
    }
    default void handleSignUpdate(ServerboundSignUpdatePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleSignUpdate");
    }
    default void handleUseItemOn(ServerboundUseItemOnPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleUseItemOn");
    }
    default void handleUseItem(ServerboundUseItemPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleUseItem");
    }
    default void handleTeleportToEntityPacket(ServerboundTeleportToEntityPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleTeleportToEntityPacket");
    }
    default void handlePaddleBoat(ServerboundPaddleBoatPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handlePaddleBoat");
    }
    default void handleMoveVehicle(ServerboundMoveVehiclePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleMoveVehicle");
    }
    default void handleAcceptTeleportPacket(ServerboundAcceptTeleportationPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleAcceptTeleportPacket");
    }
    default void handleAcceptPlayerLoad(ServerboundPlayerLoadedPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleAcceptPlayerLoad");
    }
    default void handleRecipeBookSeenRecipePacket(ServerboundRecipeBookSeenRecipePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleRecipeBookSeenRecipePacket");
    }
    default void handleBundleItemSelectedPacket(ServerboundSelectBundleItemPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleBundleItemSelectedPacket");
    }
    default void handleRecipeBookChangeSettingsPacket(ServerboundRecipeBookChangeSettingsPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleRecipeBookChangeSettingsPacket");
    }
    default void handleSeenAdvancements(ServerboundSeenAdvancementsPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleSeenAdvancements");
    }
    default void handleCustomCommandSuggestions(ServerboundCommandSuggestionPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleCustomCommandSuggestions");
    }
    default void handleSetCommandBlock(ServerboundSetCommandBlockPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleSetCommandBlock");
    }
    default void handleSetCommandMinecart(ServerboundSetCommandMinecartPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleSetCommandMinecart");
    }
    default void handlePickItemFromBlock(ServerboundPickItemFromBlockPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handlePickItemFromBlock");
    }
    default void handlePickItemFromEntity(ServerboundPickItemFromEntityPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handlePickItemFromEntity");
    }
    default void handleRenameItem(ServerboundRenameItemPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleRenameItem");
    }
    default void handleSetBeaconPacket(ServerboundSetBeaconPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleSetBeaconPacket");
    }
    default void handleSetGameRule(ServerboundSetGameRulePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleSetGameRule");
    }
    default void handleSetStructureBlock(ServerboundSetStructureBlockPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleSetStructureBlock");
    }
    default void handleSetTestBlock(ServerboundSetTestBlockPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleSetTestBlock");
    }
    default void handleTestInstanceBlockAction(ServerboundTestInstanceBlockActionPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleTestInstanceBlockAction");
    }
    default void handleSelectTrade(ServerboundSelectTradePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleSelectTrade");
    }
    default void handleEditBook(ServerboundEditBookPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleEditBook");
    }
    default void handleEntityTagQuery(ServerboundEntityTagQueryPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleEntityTagQuery");
    }
    default void handleContainerSlotStateChanged(ServerboundContainerSlotStateChangedPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleContainerSlotStateChanged");
    }
    default void handleBlockEntityTagQuery(ServerboundBlockEntityTagQueryPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleBlockEntityTagQuery");
    }
    default void handleSetJigsawBlock(ServerboundSetJigsawBlockPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleSetJigsawBlock");
    }
    default void handleJigsawGenerate(ServerboundJigsawGeneratePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleJigsawGenerate");
    }
    default void handleChangeDifficulty(ServerboundChangeDifficultyPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleChangeDifficulty");
    }
    default void handleChangeGameMode(ServerboundChangeGameModePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleChangeGameMode");
    }
    default void handleLockDifficulty(ServerboundLockDifficultyPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleLockDifficulty");
    }
    default void handleChatSessionUpdate(ServerboundChatSessionUpdatePacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleChatSessionUpdate");
    }
    default void handleConfigurationAcknowledged(ServerboundConfigurationAcknowledgedPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleConfigurationAcknowledged");
    }
    default void handleChunkBatchReceived(ServerboundChunkBatchReceivedPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleChunkBatchReceived");
    }
    default void handleDebugSubscriptionRequest(ServerboundDebugSubscriptionRequestPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleDebugSubscriptionRequest");
    }
    default void handleClientTickEnd(ServerboundClientTickEndPacket packet) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.network.protocol.game.ServerGamePacketListener#handleClientTickEnd");
    }
}
