'use client';

import { createContext, useContext, useState, useEffect, useMemo } from 'react';
import { GameProfile, 
        LedsEffectStyle, 
        Platform, GameSocdMode, 
        GameControllerButton, Hotkey, RapidTriggerConfig, GameProfileList, GlobalConfig } from '@/types/gamepad-config';
import { StepInfo, ADCValuesMapping } from '@/types/adc';

// 创建自定义fetch函数来支持Keep-Alive
const createFetchWithKeepAlive = () => {
    // 配置选项
    const defaultOptions: RequestInit = {
        keepalive: true,
        headers: {
            'Content-Type': 'application/json',
            'Connection': 'keep-alive'
        }
    };

    return async (url: string, options: RequestInit = {}): Promise<Response> => {
        // 合并默认选项和传入的选项
        const mergedOptions: RequestInit = {
            ...defaultOptions,
            ...options,
            headers: {
                ...defaultOptions.headers,
                ...options.headers
            }
        };

        return fetch(url, mergedOptions);
    };
};

// 创建全局的fetch实例
const fetchWithKeepAlive = createFetchWithKeepAlive();

// 校准状态类型定义
export interface CalibrationButtonStatus {
    index: number;
    phase: 'IDLE' | 'TOP_SAMPLING' | 'BOTTOM_SAMPLING' | 'COMPLETED' | 'ERROR';
    isCalibrated: boolean;
    topValue: number;
    bottomValue: number;
    ledColor: 'OFF' | 'RED' | 'CYAN' | 'DARK_BLUE' | 'GREEN' | 'YELLOW';
}

export interface CalibrationStatus {
    isActive: boolean;
    uncalibratedCount: number;
    activeCalibrationCount: number;
    allCalibrated: boolean;
    buttons: CalibrationButtonStatus[];
}

// 按键状态类型定义
export interface ButtonStates {
    triggerMask: number;
    triggerBinary: string;
    totalButtons: number;
    timestamp: number;
}

export interface LEDsConfig {
    ledEnabled: boolean;
    ledsEffectStyle: number;
    ledColors: string[];
    ledBrightness: number;
    ledAnimationSpeed: number;
}

interface GamepadConfigContextType {
    contextJsReady: boolean;
    setContextJsReady: (ready: boolean) => void;
    profileList: GameProfileList;
    defaultProfile: GameProfile;
    hotkeysConfig: Hotkey[];
    globalConfig: GlobalConfig;
    fetchGlobalConfig: () => Promise<void>;
    updateGlobalConfig: (globalConfig: GlobalConfig) => Promise<void>;
    fetchDefaultProfile: () => Promise<void>;
    fetchProfileList: () => Promise<void>;
    fetchHotkeysConfig: () => Promise<void>;
    updateProfileDetails: (profileId: string, profileDetails: GameProfile) => Promise<void>;
    resetProfileDetails: () => Promise<void>;
    createProfile: (profileName: string) => Promise<void>;
    deleteProfile: (profileId: string) => Promise<void>;
    switchProfile: (profileId: string) => Promise<void>;
    updateHotkeysConfig: (hotkeysConfig: Hotkey[]) => Promise<void>;
    isLoading: boolean;
    error: string | null;
    setError: (error: string | null) => void;
    rebootSystem: () => Promise<void>;
    // 校准相关
    calibrationStatus: CalibrationStatus;
    startManualCalibration: () => Promise<void>;
    stopManualCalibration: () => Promise<void>;
    fetchCalibrationStatus: () => Promise<void>;
    clearManualCalibrationData: () => Promise<void>;
    // ADC Mapping 相关
    defaultMappingId: string;
    markingStatus: StepInfo;
    mappingList: { id: string, name: string }[];
    activeMapping: ADCValuesMapping | null;
    fetchMappingList: () => Promise<void>;
    fetchDefaultMapping: () => Promise<void>;
    fetchActiveMapping: (id: string) => Promise<void>;
    createMapping: (name: string, length: number, step: number) => Promise<void>;
    deleteMapping: (id: string) => Promise<void>;
    updateDefaultMapping: (id: string) => Promise<void>;
    startMarking: (id: string) => Promise<void>;
    stopMarking: () => Promise<void>;
    stepMarking: () => Promise<void>;
    fetchMarkingStatus: () => Promise<void>;
    renameMapping: (id: string, name: string) => Promise<void>;
    // 按键监控相关
    buttonMonitoringActive: boolean;
    startButtonMonitoring: () => Promise<void>;
    stopButtonMonitoring: () => Promise<void>;
    getButtonStates: () => Promise<ButtonStates>;
    // LED 配置相关
    pushLedsConfig: (ledsConfig: LEDsConfig) => Promise<void>;
    clearLedsPreview: () => Promise<void>;
}

const GamepadConfigContext = createContext<GamepadConfigContextType | undefined>(undefined);

/**
 * convert profile details
 * @param profile - GameProfile
 * @returns 
 */
const converProfileDetails = (profile: any) => {
    const newProfile: GameProfile = {
        ...profile,
        keysConfig: {
            inputMode: profile.keysConfig?.inputMode as Platform ?? Platform.XINPUT,
            socdMode: profile.keysConfig?.socdMode as GameSocdMode ?? GameSocdMode.SOCD_MODE_UP_PRIORITY,
            invertXAxis: profile.keysConfig?.invertXAxis as boolean ?? false,
            invertYAxis: profile.keysConfig?.invertYAxis as boolean ?? false,
            fourWayMode: profile.keysConfig?.fourWayMode as boolean ?? false,
            keyMapping: profile.keysConfig?.keyMapping as { [key in GameControllerButton]?: number[] } ?? {},
        },
        ledsConfigs: {
            ledEnabled: profile.ledsConfigs?.ledEnabled as boolean ?? false,
            ledsEffectStyle: profile.ledsConfigs?.ledsEffectStyle as LedsEffectStyle ?? LedsEffectStyle.STATIC,
            ledColors: profile.ledsConfigs?.ledColors as string[] ?? ["#000000", "#000000", "#000000"],
            ledBrightness: profile.ledsConfigs?.ledBrightness as number ?? 100,
            ledAnimationSpeed: profile.ledsConfigs?.ledAnimationSpeed as number ?? 1,
        },
        hotkeys: profile.hotkeys as Hotkey[] ?? [],
        triggerConfigs: profile.triggerConfigs as { [key: number]: RapidTriggerConfig } ?? {},
    }
    return newProfile;
}

/**
 * process response
 * @param response - Response
 * @returns 
 */
const processResponse = async (response: Response, setError: (error: string | null) => void) => {
    if (!response.ok) {
        setError(response.statusText);
        return;
    }
    const data = await response.json();
    if(data.errNo) {
        setError(data.errorMessage);
        return;
    }
    return data.data;
}

/**
 * GamepadConfigProvider
 * @param children - React.ReactNode
 * @returns 
 */
export function GamepadConfigProvider({ children }: { children: React.ReactNode }) {
    const [globalConfig, setGlobalConfig] = useState<GlobalConfig>({ inputMode: Platform.XINPUT });
    const [profileList, setProfileList] = useState<GameProfileList>({ defaultId: "", maxNumProfiles: 0, items: [] });
    const [defaultProfile, setDefaultProfile] = useState<GameProfile>({ id: "", name: "" });
    const [isLoading, setIsLoading] = useState(true);
    const [error, setError] = useState<string | null>(null);
    const [hotkeysConfig, setHotkeysConfig] = useState<Hotkey[]>([]);
    const [jsReady, setJsReady] = useState(false);
    const [calibrationStatus, setCalibrationStatus] = useState<CalibrationStatus>({
        isActive: false,
        uncalibratedCount: 0,
        activeCalibrationCount: 0,
        allCalibrated: false,
        buttons: []
    });
    const [defaultMappingId, setDefaultMappingId] = useState<string>("");
    const [mappingList, setMappingList] = useState<{ id: string, name: string }[]>([]);
    const [markingStatus, setMarkingStatus] = useState<StepInfo>({
        id: "",
        mapping_name: "",
        step: 0,
        length: 0,
        index: 0,
        values: [],
        sampling_noise: 0,
        sampling_frequency: 0,
        is_marking: false,
        is_sampling: false,
        is_completed: false
    });
    const [activeMapping, setActiveMapping] = useState<ADCValuesMapping | null>(null);
    const [buttonMonitoringActive, setButtonMonitoringActive] = useState<boolean>(false);

    const contextJsReady = useMemo(() => {
        return jsReady;
    }, [jsReady]);

    useEffect(() => {
        fetchGlobalConfig();
        fetchProfileList();
        fetchHotkeysConfig();
    }, []);

    useEffect(() => {
        if (profileList.defaultId !== "") {
            fetchDefaultProfile();
        }
    }, [profileList]);

    const setContextJsReady = (ready: boolean) => {
        setJsReady(ready);
    }

    const fetchDefaultProfile = async (): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/default-profile', {
                method: 'GET'
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to fetch default profile"));
            };
            setDefaultProfile(converProfileDetails(data.profileDetails) ?? {});
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to fetch default profile"));
        } finally {
            setIsLoading(false);
        }
    };

    const fetchProfileList = async (): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/profile-list', {
                method: 'GET'
            }); 
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to fetch profile list"))    ;
            };
            setProfileList(data.profileList);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to fetch profile list"));
        } finally {
            setIsLoading(false);
        }
    };

    const fetchHotkeysConfig = async (): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/hotkeys-config', {
                method: 'GET'
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to fetch hotkeys config"));
            };
            setHotkeysConfig(data.hotkeysConfig);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to fetch hotkeys config"));
        } finally {
            setIsLoading(false);
        }
    }

    const updateProfileDetails = async (profileId: string, profileDetails: GameProfile): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/update-profile', {
                method: 'POST',
                body: JSON.stringify({ profileId, profileDetails }),
            });

            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to update profile details"));
            };

            // 如果更新的是 profile 的 name，则需要重新获取 profile list
            if(profileDetails.hasOwnProperty("name") || profileDetails.hasOwnProperty("id")) {
                fetchProfileList();
            } else { // 否则更新 default profile
                setDefaultProfile(converProfileDetails(data.profileDetails) ?? {});
            }
            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to update profile details"));
        } finally {
            setIsLoading(false);
        }
    };

    const resetProfileDetails = async (): Promise<void> => {
        await fetchDefaultProfile();
    };

    const createProfile = async (profileName: string): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/create-profile', {
                method: 'POST',
                body: JSON.stringify({ profileName }),
            });

            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to create profile"));
            };
            setProfileList(data.profileList);
            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to create profile"));
        } finally {
            setIsLoading(false);
        }
    };

    const deleteProfile = async (profileId: string): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/delete-profile', {
                method: 'POST',
                body: JSON.stringify({ profileId }),
            });

            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to delete profile"));
            };
            setProfileList(data.profileList);
            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to delete profile"));
        } finally {
            setIsLoading(false);
        }
    };

    const switchProfile = async (profileId: string): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/switch-default-profile', {
                method: 'POST',
                body: JSON.stringify({ profileId }),
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to switch profile"));
            };
            setProfileList(data.profileList);
            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to switch profile"));
        } finally {
            setIsLoading(false);
        }
    };

    const updateHotkeysConfig = async (hotkeysConfig: Hotkey[]): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/update-hotkeys-config', {
                method: 'POST',
                body: JSON.stringify({ hotkeysConfig }),
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to update hotkeys config"));
            }
            setHotkeysConfig(data.hotkeysConfig);
            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to update hotkeys config"));
        } finally {
            setIsLoading(false);
        }
    };

    const rebootSystem = async (): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/reboot', {
                method: 'POST',
            });

            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to reboot system"));
            }
            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to reboot system"));
        } finally {
            setIsLoading(false);
        }
    };

    const fetchMappingList = async (): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/ms-get-list', {
                method: 'GET'
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to fetch mapping list"));
            }
            setMappingList(data.mappingList);
            setDefaultMappingId(data.defaultMappingId);
            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to fetch mapping list"));
        } finally {
            setIsLoading(false);
        }
    };

    const fetchDefaultMapping = async (): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/ms-get-default', {
                method: 'GET'
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to fetch default mapping"));
            }
            setDefaultMappingId(data.id ?? "");
            return Promise.resolve(data.id);
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to fetch default mapping"));
        } finally {
            setIsLoading(false);
        }
    };

    const createMapping = async (name: string, length: number, step: number): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/ms-create-mapping', {
                method: 'POST',
                body: JSON.stringify({ name, length, step }),
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to create mapping"));
            }
            setMappingList(data.mappingList);
            setDefaultMappingId(data.defaultMappingId);
            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to create mapping"));
        } finally {
            setIsLoading(false);
        }
    };

    const deleteMapping = async (id: string): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/ms-delete-mapping', {
                method: 'POST',
                body: JSON.stringify({ id }),
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to delete mapping"));
            }
            setMappingList(data.mappingList);
            setDefaultMappingId(data.defaultMappingId);
            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to delete mapping"));
        } finally {
            setIsLoading(false);
        }
    };

    const updateDefaultMapping = async (id: string): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/ms-set-default', {
                method: 'POST',
                body: JSON.stringify({ id }),
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to set default mapping"));
            }
            setDefaultMappingId(data.id);
            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to set default mapping"));
        } finally {
            setIsLoading(false);
        }
    };

    const startMarking = async (id: string): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/ms-mark-mapping-start', {
                method: 'POST',
                body: JSON.stringify({ id }),
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to start marking"));
            }

            if(data.status) {
                setMarkingStatus(data.status);
            }

            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to start marking"));
        } finally {
            setIsLoading(false);
        }
    };

    const stopMarking = async (): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/ms-mark-mapping-stop', {
                method: 'GET'
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to stop marking"));
            }

            if(data.status) {
                setMarkingStatus(data.status);
            }

            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to stop marking"));
        } finally {
            setIsLoading(false);
        }
    };

    const stepMarking = async (): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/ms-mark-mapping-step', {
                method: 'GET'
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to step marking"));
            }

            if(data.status) {
                setMarkingStatus(data.status);
            }

            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to step marking"));
        } finally {
            setIsLoading(false);
        }
    };

    const fetchMarkingStatus = async (): Promise<void> => {
        try {
            const response = await fetchWithKeepAlive('/api/ms-get-mark-status', {
                method: 'GET'
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to fetch marking status"));
            }

            if(data.status) {
                setMarkingStatus(data.status);
            }

            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to fetch marking status"));
        }
    };

    const fetchActiveMapping = async (id: string): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/ms-get-mapping', {
                method: 'POST',
                body: JSON.stringify({ id }),
            });

            const data = await processResponse(response, setError);

            if (!data) {
                return Promise.reject(new Error("Failed to fetch mapping"));
            }

            setActiveMapping(data.mapping);

            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to fetch mapping"));
        } finally {
            setIsLoading(false);
        }
    };

    const renameMapping = async (id: string, name: string): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/ms-rename-mapping', {
                method: 'POST',
                body: JSON.stringify({ id, name }),
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to rename mapping"));
            }
            setMappingList(data.mappingList);
            setDefaultMappingId(data.defaultMappingId);
            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to rename mapping"));
        } finally {
            setIsLoading(false);
        }
    };

    const fetchGlobalConfig = async (): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/global-config', {
                method: 'GET'
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to fetch global config"));
            }
            setGlobalConfig(data.globalConfig);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to fetch global config"));
        } finally {
            setIsLoading(false);
        }
    };

    const updateGlobalConfig = async (globalConfig: GlobalConfig): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/update-global-config', {
                method: 'POST',
                body: JSON.stringify({ globalConfig }),
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to update global config"));
            }
            setGlobalConfig(data.globalConfig);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to update global config"));
        } finally {
            setIsLoading(false);
        }
    };

    const startManualCalibration = async (): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/start-manual-calibration', {
                method: 'GET'
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to start manual calibration"));
            }
            setCalibrationStatus(data.calibrationStatus);
            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to start manual calibration"));
        } finally {
            setIsLoading(false);
        }
    };

    const stopManualCalibration = async (): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/stop-manual-calibration', {
                method: 'GET'
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to stop manual calibration"));
            }
            setCalibrationStatus(data.calibrationStatus);
            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to stop manual calibration"));
        } finally {
            setIsLoading(false);
        }
    };

    const fetchCalibrationStatus = async (): Promise<void> => {
        try {
            const response = await fetchWithKeepAlive('/api/get-calibration-status', {
                method: 'GET'
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to fetch calibration status"));
            }
            setCalibrationStatus(data.calibrationStatus);
            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to fetch calibration status"));
        }
    };

    const clearManualCalibrationData = async (): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/clear-manual-calibration-data', {
                method: 'GET'
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to clear manual calibration data"));
            }
            setCalibrationStatus(data.calibrationStatus);
            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to clear manual calibration data"));
        } finally {
            setIsLoading(false);
        }
    };

    const startButtonMonitoring = async (): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/start-button-monitoring', {
                method: 'GET'
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to start button monitoring"));
            }
            setButtonMonitoringActive(data.isActive ?? true);
            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to start button monitoring"));
        } finally {
            setIsLoading(false);
        }
    };

    const stopButtonMonitoring = async (): Promise<void> => {
        try {
            setIsLoading(true);
            const response = await fetchWithKeepAlive('/api/stop-button-monitoring', {
                method: 'GET'
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to stop button monitoring"));
            }
            setButtonMonitoringActive(data.isActive ?? false);
            setError(null);
            return Promise.resolve();
        } catch (err) {
            setError(err instanceof Error ? err.message : 'An error occurred');
            return Promise.reject(new Error("Failed to stop button monitoring"));
        } finally {
            setIsLoading(false);
        }
    };

    const getButtonStates = async (): Promise<ButtonStates> => {
        setError(null);
        try {
            const response = await fetchWithKeepAlive('/api/get-button-states');
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to get button states"));
            }
            return Promise.resolve(data as ButtonStates);
        } catch (error) {
            console.error('获取按键状态失败:', error);
            setError(error instanceof Error ? error.message : '获取按键状态失败');
            throw error;
        }
    };

    // LED 配置相关
    const pushLedsConfig = async (ledsConfig: LEDsConfig): Promise<void> => {
        setError(null);
        // setIsLoading(true);
        try {
            const response = await fetchWithKeepAlive('/api/push-leds-config', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(ledsConfig),
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to push LED configuration"));
            }
            return Promise.resolve();
        } catch (error) {
            setError(error instanceof Error ? error.message : 'An error occurred');
            return Promise.reject(new Error("Failed to push LED configuration"));
        } finally {
            // setIsLoading(false);
        }
    };

    const clearLedsPreview = async (): Promise<void> => {
        setError(null);
        try {
            const response = await fetchWithKeepAlive('/api/clear-leds-preview', {
                method: 'GET'
            });
            const data = await processResponse(response, setError);
            if (!data) {
                return Promise.reject(new Error("Failed to clear LED preview"));
            }
            return Promise.resolve();
        } catch (error) {
            setError(error instanceof Error ? error.message : 'An error occurred');
            return Promise.reject(new Error("Failed to clear LED preview"));
        }
    };
    
    

    return (
        <GamepadConfigContext.Provider value={{
            contextJsReady,
            setContextJsReady,
            globalConfig,
            profileList,
            defaultProfile,
            hotkeysConfig,
            fetchDefaultProfile,
            fetchProfileList,
            fetchHotkeysConfig,
            fetchGlobalConfig,
            updateGlobalConfig,
            updateProfileDetails,
            resetProfileDetails,
            createProfile,
            deleteProfile,
            switchProfile,
            updateHotkeysConfig,
            isLoading,
            error,
            setError,
            rebootSystem,
            // 校准相关
            calibrationStatus,
            startManualCalibration,
            stopManualCalibration,
            fetchCalibrationStatus,
            clearManualCalibrationData,
            // ADC Mapping 相关
            defaultMappingId: defaultMappingId,
            markingStatus,
            mappingList,
            activeMapping,
            fetchMappingList,
            fetchMarkingStatus,
            updateDefaultMapping,
            fetchDefaultMapping,
            fetchActiveMapping,
            createMapping,
            deleteMapping,
            startMarking,
            stopMarking,
            stepMarking,
            renameMapping,
            // 按键监控相关
            buttonMonitoringActive: buttonMonitoringActive,
            startButtonMonitoring,
            stopButtonMonitoring,
            getButtonStates,
            // LED 配置相关
            pushLedsConfig: pushLedsConfig,
            clearLedsPreview: clearLedsPreview,
        }}>
            {children}
        </GamepadConfigContext.Provider>
    );
}

export function useGamepadConfig() {
    const context = useContext(GamepadConfigContext);
    if (context === undefined) {
        throw new Error('useGamepadConfig must be used within a GamepadConfigProvider');
    }
    return context;
} 