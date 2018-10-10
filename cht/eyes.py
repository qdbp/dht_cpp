import matplotlib

# breaker

matplotlib.use('cairo')


def main():
    import matplotlib.pyplot as plt
    import numpy as np
    import pandas as pd
    from cycler import cycler

    plt.style.use('ggplot')

    df = pd.read_csv('./data/stat.csv').iloc[:, :-1]

    df['time'] -= df['time'][0]
    dt = df['time'][1] - df['time'][0]
    df.set_index('time', inplace=True)
    df /= dt

    df['rx_q_tot'] = df[['rx_q_ap', 'rx_q_fn',
                         'rx_q_gp', 'rx_q_pg']].sum(axis=1)
    df['rx_r_tot'] = df[['rx_r_ap', 'rx_r_fn',
                         'rx_r_gp', 'rx_r_pg']].sum(axis=1)
    df['rx_tot'] = df[['rx_r_tot', 'rx_q_tot']].sum(axis=1)

    df['tx_q_tot'] = df[['tx_q_fn', 'tx_q_gp', 'tx_q_pg']].sum(axis=1)
    df['tx_r_tot'] = df[['tx_r_ap', 'tx_r_fn',
                         'tx_r_gp', 'tx_r_pg']].sum(axis=1)
    df['tx_tot'] = df[['tx_r_tot', 'tx_q_tot']].sum(axis=1)

    df = df.diff().iloc[1:, :] + 1

    print(df[['rx_r_tot', 'tx_q_tot']])

    ax = plt.gca()
    plt.yscale('log')
    plt.ylim(1, 1e5)

    df.plot(ax=ax, y='tx_q_tot', color='k', lw=2.5)
    df.plot(ax=ax, y='tx_q_gp', color='#0060cc')
    df.plot(ax=ax, y='tx_q_pg', color='gray')
    df.plot(ax=ax, y='tx_q_fn', color='#402070')

    df.plot(ax=ax, y='rx_r_tot', color='k', lw=2.5, linestyle='--')
    df.plot(ax=ax, y='rx_r_gp_nodes', color='#0080aa', linestyle='--')
    df.plot(ax=ax, y='rx_r_gp_values', color='#0040ee', linestyle='--')
    df.plot(ax=ax, y='rx_r_fn', color='#402070', linestyle='--')
    df.plot(ax=ax, y='rx_r_pg', color='gray', linestyle='--')

    df.plot(ax=ax, y='rx_q_tot', color='k', lw=2.5, linestyle=':')
    df.plot(ax=ax, y='rx_q_pg', color='#660000', linestyle=':')
    df.plot(ax=ax, y='rx_q_gp', color='#dd0000', linestyle=':')
    df.plot(ax=ax, y='rx_q_fn', color='orange', linestyle=':')
    df.plot(ax=ax, y='rx_q_ap', color='#bb9000', linestyle=':')

    df.plot(ax=ax, y='rx_spam', color='red', label='rx spam')
    df.plot(ax=ax, y='tx_ping_drop_spam', color='red',
            label='ping spam', linestyle='--')
    df.plot(ax=ax, y='tx_q_gp_drop_spam', color='red',
            label='q_gp spam', linestyle=':')

    plt.legend()
    plt.gcf().set_size_inches((1920/150, 1080/150))
    plt.gcf().set_dpi(150)
    plt.tight_layout()
    plt.savefig('plot.png')


if __name__ == '__main__':
    main()
