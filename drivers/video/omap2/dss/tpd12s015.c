int 

static int __init omapdss_tpd12s015_probe (platform device *pdev)
{
}
int __init tpd12s015_init_platform_driver(void)
{
	return platform_driver_probe(&omapdss_tpd12s015_driver, omapdss_tpd12s015_probe);
}

void __exit tpd12s015_uninit_platform_driver(void)
{
	platform_driver_unregister(&omapdss_hdmihw_driver);
}
