"use client";

import {
    Flex,
    Center,
    Stack,
    Fieldset,
    SimpleGrid,
    Button,
    HStack,
    RadioCardLabel,
    Text,
    Card,
    VStack,
    Switch,
} from "@chakra-ui/react";
import KeymappingFieldset from "@/components/keymapping-fieldset";
import { useEffect, useState } from "react";
import {
    RadioCardItem,
    RadioCardRoot,
} from "@/components/ui/radio-card"
import {
    GameProfile,
    GameSocdMode,
    GameSocdModeLabelMap,
    GameControllerButton,
    KEYS_SETTINGS_INTERACTIVE_IDS,
    Platform,
} from "@/types/gamepad-config";
import { SegmentedControl } from "@/components/ui/segmented-control";
import Hitbox from "@/components/hitbox";
import { LuInfo } from "react-icons/lu";
import { ToggleTip } from "@/components/ui/toggle-tip"
import { useGamepadConfig } from "@/contexts/gamepad-config-context";
import useUnsavedChangesWarning from "@/hooks/use-unsaved-changes-warning";
import { useLanguage } from "@/contexts/language-context";
import { ContentActionButtons } from "@/components/content-action-buttons";
import { useColorMode } from "./ui/color-mode";
import { ProfileSelect } from "@/components/profile-select";
import { useButtonMonitor } from "@/hooks/use-button-monitor";
import type { ButtonEvent } from "@/components/button-monitor-manager";

export function KeysSettingContent() {
    const {
        defaultProfile,
        updateProfileDetails,
        fetchDefaultProfile,
        globalConfig,
    } = useGamepadConfig();
    const [_isDirty, setIsDirty] = useUnsavedChangesWarning();
    const { t } = useLanguage();
    const { colorMode } = useColorMode();
    
    // 按键映射状态
    const [socdMode, setSocdMode] = useState<GameSocdMode>(GameSocdMode.SOCD_MODE_UP_PRIORITY);
    const [invertXAxis, setInvertXAxis] = useState<boolean>(false);
    const [invertYAxis, setInvertYAxis] = useState<boolean>(false);
    const [fourWayMode, setFourWayMode] = useState<boolean>(false);
    const [keyMapping, setKeyMapping] = useState<{ [key in GameControllerButton]?: number[] }>({});
    const [autoSwitch, setAutoSwitch] = useState<boolean>(true);
    const [inputKey, setInputKey] = useState<number>(-1);


    // 使用新的按键监控 hook
    const buttonMonitor = useButtonMonitor({
        pollingInterval: 500,
        onError: (error) => {
            console.error('按键监控错误:', error);
        },
    });

    useEffect(() => {
        if (defaultProfile.keysConfig) {
            setSocdMode(defaultProfile.keysConfig?.socdMode ?? GameSocdMode.SOCD_MODE_UP_PRIORITY);
            setInvertXAxis(defaultProfile.keysConfig?.invertXAxis ?? false);
            setInvertYAxis(defaultProfile.keysConfig?.invertYAxis ?? false);
            setFourWayMode(defaultProfile.keysConfig?.fourWayMode ?? false);
            setKeyMapping(defaultProfile.keysConfig?.keyMapping ?? {});
            setIsDirty?.(false); // reset the unsaved changes warning 
        }
    }, [defaultProfile, setIsDirty]);

    // 处理监控状态切换
    useEffect(() => {
        buttonMonitor.startMonitoring();
        return () => {
            buttonMonitor.stopMonitoring();
        };
    }, []);

    // 监听设备按键事件
    useEffect(() => {
        const handleDeviceButtonEvent = (event: CustomEvent<ButtonEvent>) => {
            const buttonEvent = event.detail;
            
            console.log('Received device button event:', {
                type: buttonEvent.type,
                buttonIndex: buttonEvent.buttonIndex,
                timestamp: Date.now()
            });
            
            // 只处理按键按下事件，并且只在启用监控时
            if (buttonEvent.type === 'button-press') {
                // 设置输入按键，触发 KeymappingFieldset 的处理
                setInputKey(buttonEvent.buttonIndex);
            } else if (buttonEvent.type === 'button-release') {
                setInputKey(-1);
            }
        };
        
        // 添加事件监听器
        window.addEventListener('device-button-event', handleDeviceButtonEvent as EventListener);

        // 清理函数
        return () => {
            window.removeEventListener('device-button-event', handleDeviceButtonEvent as EventListener);
        };
    }, []);

    
    /**
     * set key mapping
     * @param key - game controller button
     * @param hitboxButtons - hitbox buttons
     */
    const setHitboxButtons = (key: string, hitboxButtons: number[]) => {
        setKeyMapping({
            ...keyMapping,
            [key as GameControllerButton]: hitboxButtons,
        });
    }

    const hitboxButtonClick = (keyId: number) => {
        setInputKey(keyId);
    }



    const saveProfileDetailHandler = (): Promise<void> => {
        const newProfile: GameProfile = {
            id: defaultProfile.id,
            keysConfig: {
                invertXAxis: invertXAxis,
                invertYAxis: invertYAxis,
                fourWayMode: fourWayMode,
                socdMode: socdMode,
                keyMapping: keyMapping,
            },
        }

        return updateProfileDetails(defaultProfile.id, newProfile);
    }

    return (
        <Flex direction="row" width={"100%"} height={"100%"} padding={"18px"} >
            <Center >
                <ProfileSelect />
            </Center>
            <Center flex={1}  >
                <Hitbox
                    onClick={hitboxButtonClick}
                    interactiveIds={KEYS_SETTINGS_INTERACTIVE_IDS}
                />
            </Center>
            <Center>
                <Card.Root w="778px" h="100%" >
                    <Card.Header>
                        <Card.Title fontSize={"md"}  >
                            <Text fontSize={"32px"} fontWeight={"normal"} color={"green.600"} >{t.SETTINGS_KEYS_TITLE}</Text>
                        </Card.Title>
                        <Card.Description fontSize={"sm"} pt={4} pb={4} whiteSpace="pre-wrap"  >
                            {t.SETTINGS_KEYS_HELPER_TEXT}
                        </Card.Description>
                    </Card.Header>
                    <Card.Body>
                        <Fieldset.Root  >
                            <Fieldset.Content >
                                <VStack gap={8} alignItems={"flex-start"} >
                                    {/* Key Mapping */}
                                    <Stack direction={"column"}>
                                        <Fieldset.Legend fontSize={"sm"} fontWeight={"bold"} >{t.SETTINGS_KEYS_MAPPING_TITLE}</Fieldset.Legend>
                                        <HStack gap={1} >
                                            <SegmentedControl
                                                size={"xs"}
                                                defaultValue={autoSwitch ? t.SETTINGS_KEY_MAPPING_AUTO_SWITCH_LABEL : t.SETTINGS_KEY_MAPPING_MANUAL_SWITCH_LABEL}
                                                items={[t.SETTINGS_KEY_MAPPING_AUTO_SWITCH_LABEL, t.SETTINGS_KEY_MAPPING_MANUAL_SWITCH_LABEL]}
                                                onValueChange={(detail) => setAutoSwitch(detail.value === t.SETTINGS_KEY_MAPPING_AUTO_SWITCH_LABEL)}
                                            />
                                            <ToggleTip content={t.TOOLTIP_AUTO_SWITCH}  >
                                                <Button size="xs" variant="ghost">
                                                    <LuInfo />
                                                </Button>
                                            </ToggleTip>
                                        </HStack>
                                        
                                        <KeymappingFieldset
                                            autoSwitch={autoSwitch}
                                            inputKey={inputKey}
                                            inputMode={globalConfig.inputMode ?? Platform.XINPUT}
                                            keyMapping={keyMapping}
                                            changeKeyMappingHandler={(key, hitboxButtons) => {
                                                setHitboxButtons(key, hitboxButtons);
                                                setIsDirty?.(true);
                                            }}
                                        />
                                    </Stack>

                                    {/* SOCD Mode Choice */}
                                    <RadioCardRoot
                                        colorPalette={"green"}
                                        size={"sm"}
                                        variant={colorMode === "dark" ? "surface" : "outline"}
                                        value={socdMode?.toString() ?? GameSocdMode.SOCD_MODE_UP_PRIORITY.toString()}
                                        onValueChange={(detail) => {
                                            setSocdMode(parseInt(detail.value ?? GameSocdMode.SOCD_MODE_NEUTRAL.toString()) as GameSocdMode);
                                            setIsDirty?.(true);
                                        }}
                                    >
                                        <RadioCardLabel>{t.SETTINGS_KEYS_SOCD_MODE_TITLE}</RadioCardLabel>
                                        <SimpleGrid gap={1} columns={5} >
                                            {Array.from({ length: GameSocdMode.SOCD_MODE_NUM_MODES }, (_, index) => (
                                                <RadioCardItem
                                                    fontSize={"xs"}
                                                    indicator={false}
                                                    key={index}
                                                    value={index.toString()}
                                                    label={GameSocdModeLabelMap.get(index as GameSocdMode)?.label ?? ""}
                                                />
                                            ))}
                                        </SimpleGrid>
                                    </RadioCardRoot>
                                    
                                    {/* Invert Axis Choice & Invert Y Axis Choice & FourWay Mode Choice */}
                                    <HStack gap={5}>
                                        <Switch.Root
                                            colorPalette={"green"}
                                            checked={invertXAxis}
                                            onCheckedChange={() => {
                                                setInvertXAxis(!invertXAxis);
                                                setIsDirty?.(true);
                                            }}
                                        >
                                            <Switch.HiddenInput />
                                            <Switch.Control>
                                                <Switch.Thumb />
                                            </Switch.Control>
                                            <Switch.Label>{t.SETTINGS_KEYS_INVERT_X_AXIS}</Switch.Label>
                                        </Switch.Root>

                                        <Switch.Root
                                            colorPalette={"green"}
                                            checked={invertYAxis}
                                            onCheckedChange={() => {
                                                setInvertYAxis(!invertYAxis);
                                                setIsDirty?.(true);
                                            }}
                                        >
                                            <Switch.HiddenInput />
                                            <Switch.Control>
                                                <Switch.Thumb />
                                            </Switch.Control>
                                            <Switch.Label>{t.SETTINGS_KEYS_INVERT_Y_AXIS}</Switch.Label>
                                        </Switch.Root>
                                    </HStack>  
                                </VStack>
                            </Fieldset.Content>
                        </Fieldset.Root>
                    </Card.Body>

                    <Card.Footer justifyContent={"flex-start"} >
                        <ContentActionButtons
                            isDirty={_isDirty}
                            resetLabel={t.BUTTON_RESET}
                            saveLabel={t.BUTTON_SAVE}
                            resetHandler={fetchDefaultProfile}
                            saveHandler={saveProfileDetailHandler}
                        />
                    </Card.Footer>
                </Card.Root>
            </Center>
        </Flex>
    )
}
