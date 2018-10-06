import matplotlib

# breaker

matplotlib.use('cairo')


def main():
    import matplotlib.pyplot as plt
    import numpy as np
    import pandas as pd

    plt.style.use('ggplot')

    df = pd.read_csv('./data/stat.csv').iloc[:, :-1]
    df.set_index('time', inplace=True)

    del df['ctl_ping_thresh']

    cols = df.columns.copy()

    for col in cols:
        if ('rx_q_' not in col) and ('tx_r_' not in col) \
                and ('rx_r_' not in col) and ('tx_q_' not in col) \
                or ('_ap' in col) or ('tx_r_' in col):

            del df[col]

    df = df.diff() + 1
    df.plot()

    plt.yscale('log')
    plt.legend()
    plt.gcf().set_size_inches((1920/150, 1080/150))
    plt.gcf().set_dpi(150)
    plt.tight_layout()
    plt.savefig('plot.png')


if __name__ == '__main__':
    main()
