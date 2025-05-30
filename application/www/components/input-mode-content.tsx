'use client';

import { PlatformLabelMap, Platform } from '@/types/gamepad-config';
import { useGamepadConfig } from '@/contexts/gamepad-config-context';
import { Center, HStack, Icon, RadioCard } from '@chakra-ui/react';
import { BsXbox } from "react-icons/bs";
import { SiNintendoswitch, SiPlaystation4, SiPlaystation5   } from "react-icons/si";


export function InputModeSettingContent() {
    const { globalConfig, updateGlobalConfig } = useGamepadConfig();

    const platformIcons = new Map<Platform, { icon: React.ReactNode, size: string }>([
        [Platform.XINPUT, { icon: <BsXbox />, size: "4xl" }],
        [Platform.PS4, { icon: <SiPlaystation4 />, size: "6xl" }],
        [Platform.PS5, { icon: <SiPlaystation5 />, size: "6xl" }],
        [Platform.SWITCH, { icon: <SiNintendoswitch />, size: "4xl" }],
    ]);

    return (
        <Center w="100%" >
            <RadioCard.Root 
                value={globalConfig.inputMode} 
                orientation="vertical"
                align="center"
                w="750px"
                variant={"solid"}
                colorPalette={"green"}
                onValueChange={(detail) => {
                    updateGlobalConfig({ inputMode: detail.value as Platform });
                }}
            >
                <HStack w="100%" justifyContent="center" >
                    {Array.from(PlatformLabelMap.entries()).map(([platform, { label }]) => (
                        <RadioCard.Item key={platform} value={platform} >
                            <RadioCard.ItemHiddenInput />
                            <RadioCard.ItemControl>
                                <Center h="70px" >
                                    <Icon fontSize={platformIcons.get(platform as Platform)?.size} color={globalConfig.inputMode === platform ? "white" : "fg.muted"}>
                                        {platformIcons.get(platform as Platform)?.icon}
                                    </Icon>
                                </Center>
                                <RadioCard.ItemText>{label}</RadioCard.ItemText>
                            </RadioCard.ItemControl>
                        </RadioCard.Item>
                    ))}
                </HStack>
            </RadioCard.Root>
        </Center>
    );
}

